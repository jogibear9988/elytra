/** Realm-owned listeners and native event credit; no principal-world callback or raw host export.
 */
#include "IpcEndpoint.h"
#include "ports/PortEndpoint.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include <AK/JsonObject.h>
#include <AK/NumericLimits.h>
#include <LibCore/EventLoop.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/FunctionObject.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/Page/Page.h>
#include <WebContent/PageClient.h>

namespace Firefly {
void IpcEndpoint::control(String const& kind, u32 request)
{
    if (!m_document || !m_owner || m_owner->is_detached() || !m_document->is_fully_active())
        return;
    auto nav = m_document->navigable();
    auto* page = nav ? as_if<WebContent::PageClient>(nav->page().client()) : nullptr;
    if (!page || !nav->is_top_level_traversable())
        return;
    JsonObject envelope;
    envelope.set("document"sv,
        MUST(String::formatted("{}:{}", m_document->id().namespace_id, m_document->id().local_id)));
    envelope.set("realm"sv, m_realm);
    envelope.set("request"sv, request);
    envelope.set("wire"sv, String {});
    page->webdriver_bidi_event(nav->id(), kind, move(envelope));
}

JS::ThrowCompletionOr<JS::Value> IpcEndpoint::listen(JS::VM& vm, ListenerOperation mode)
{
    auto failure = [&](Utf16View code, Utf16View message) -> JS::ThrowCompletionOr<JS::Value> {
        auto error = JS::Error::create(*vm.current_realm(), message);
        error->define_direct_property(
            "code"_utf16, JS::PrimitiveString::create(vm, code), JS::default_attributes);
        return JS::throw_completion(error);
    };

    if (!m_owner || m_owner->is_detached() || !m_owner->is_executing()
        || m_owner->realm() != vm.current_realm() || !m_document->is_fully_active())
        return failure(u"ERR_FIREFLY_IPC_REVOKED"sv, u"IPC listener owner is inactive"sv);
    if (mode == ListenerOperation::RemoveAll && vm.argument(0).is_undefined()) {
        m_listeners.clear();
        return m_exports.ptr();
    }

    if (!vm.argument(0).is_string()
        || vm.argument(0).as_string().length_in_utf16_code_units() > 256)
        return failure(
            u"ERR_INVALID_ARG_TYPE"sv, u"IPC listener channel must be a bounded string"sv);
    auto channel = vm.argument(0).as_string().utf16_string();
    if (mode == ListenerOperation::RemoveAll) {
        m_listeners.remove_all_matching(
            [&](auto const& entry) { return entry.channel == channel; });
        return m_exports.ptr();
    }

    if (!vm.argument(1).is_function())
        return failure(u"ERR_INVALID_ARG_TYPE"sv, u"IPC listener must be callable"sv);
    auto& callback = vm.argument(1).as_function();
    if (&callback.vm() != &vm || callback.realm() != m_owner->realm())
        return failure(u"ERR_FIREFLY_IPC_SERIALIZATION"sv,
            u"IPC listener requires the owning isolated realm"sv);
    if (mode == ListenerOperation::Remove) {
        // Node removes the most recently registered matching occurrence.
        for (size_t i = m_listeners.size(); i > 0; --i) {
            auto const& entry = m_listeners[i - 1];
            if (entry.channel == channel && entry.callback.ptr() == &callback) {
                m_listeners.remove(i - 1);
                break;
            }
        }

        return m_exports.ptr();
    }

    if (m_listeners.size() >= 64 || m_listener_next == NumericLimits<u64>::max())
        return failure(u"ERR_FIREFLY_IPC_LIMIT"sv, u"IPC listener budget exceeded"sv);
    m_listeners.append({ move(channel), GC::Root { callback }, ++m_listener_next,
        mode == ListenerOperation::Once });
    return m_exports.ptr();
}

void IpcEndpoint::receive_event(u32 request, ByteString wire,
    RefPtr<IpcWireReservation> reservation, Vector<Utility::Ports::Identity> ports)
{
    if (m_owner && m_owner->is_executing()) {
        // Core can be pumped by native work while a source frame is active.
        // Keep only bounded native message data until entering a fresh world frame.
        Core::deferred_invoke([self = NonnullRefPtr { *this }, request, wire = move(wire),
                                  reservation = move(reservation), ports = move(ports)]() mutable {
            self->receive_event(request, move(wire), move(reservation), move(ports));
        });
        return;
    }

    VERIFY(m_incoming_pending);
    --m_incoming_pending;
    if (m_byte_failed || !m_owner || m_owner->is_detached() || !m_document
        || !m_document->is_fully_active())
        return;
    auto owner = m_owner;
    Web::HTML::TemporaryExecutionContext context(m_document->relevant_settings_object());
    auto result = owner->run_async_frame([&]() -> JS::ThrowCompletionOr<JS::Value> {
        auto& realm = *owner->realm();
        auto& vm = realm.vm();
        auto decoded = BridgeData::decode_wire(wire.view(), application_ipc_limits());
        if (decoded.is_error())
            return vm.throw_completion<JS::TypeError>("Invalid IPC event graph"_utf16);
        auto copied
            = TRY(decoded.value().materialize(realm, {}, BridgeData::Materialization::Mutable));
        auto args = GC::Root<JS::Value>::create(copied, SourceLocation::current());
        if (!copied.is_object() || copied.as_object().engine_class_id() != JS_LAYOUT_CLASS_ID_ARRAY)
            return vm.throw_completion<JS::TypeError>("Invalid IPC event arguments"_utf16);
        auto& array = copied.as_object();
        auto length = TRY(TRY(array.get("length"_utf16)).to_length(vm));
        auto channel = TRY(array.get(0));
        if (length < 1 || length > 65 || !channel.is_string()
            || channel.as_string().length_in_utf16_code_units() > 256)
            return vm.throw_completion<JS::TypeError>("Invalid IPC event channel"_utf16);
        auto name = channel.as_string().utf16_string();
        // Snapshot registrations before callbacks. Removals during an emit do not
        // erase callbacks already selected for that emit; additions wait for the next.
        Vector<Listener> listeners;
        for (auto const& entry : m_listeners)
            if (entry.channel == name)
                listeners.append(
                    { entry.channel, GC::Root { *entry.callback }, entry.id, entry.once });
        GC::Root event { JS::Object::create(realm, realm.intrinsics().object_prototype()) };
        event->define_direct_property("sender"_utf16, m_exports.ptr(), JS::default_attributes);
        event->define_direct_property("ports"_utf16,
            ports.is_empty() ? JS::Value(TRY(JS::Array::create(realm, 0)))
                             : TRY(m_ports->materialize(realm, ports)),
            JS::default_attributes);
        GC::RootVector<JS::Value> arguments;
        arguments.append(event.ptr());
        for (size_t i = 1; i < length; ++i)
            arguments.append(TRY(array.get(i)));
        for (auto& entry : listeners) {
            if (!m_owner || m_owner->is_detached() || !m_document->is_fully_active())
                break;
            if (entry.once)
                m_listeners.remove_all_matching(
                    [&](auto const& current) { return current.id == entry.id; });
            TRY(JS::call(vm, *entry.callback, m_exports.ptr(), arguments.span()));
        }

        return JS::js_undefined();
    });
    if (result.is_error())
        warnln("ERR_FIREFLY_IPC_LISTENER: renderer event dispatch failed");
    // Application exceptions do not retain credit. This acknowledges transport
    // dispatch, never the result of a listener or its returned Promise.
    if (ports.is_empty())
        send_packet(ApplicationIpc::Kind::Ack, request, false, {});
    else
        m_ports->acknowledge_transfer(request);
}
}
