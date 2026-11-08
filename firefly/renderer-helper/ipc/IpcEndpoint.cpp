/** Preload-only invoke/event endpoint. Own Promise roots or private event credit until
 * settlement, timeout or document revocation; accept bounded data graphs only. */
#include "IpcEndpoint.h"
#include "serialization/BridgeData.h"
#include <AK/JsonObject.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/PromiseConstructor.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/Page/Page.h>
#include <WebContent/PageClient.h>

namespace Firefly {
namespace {
    constexpr size_t wire_limit = 128 * 1024;
    size_t pending_count;

    String document_key(Web::DOM::Document const& doc)
    {
        return MUST(String::formatted("{}:{}", doc.id().namespace_id, doc.id().local_id));
    }

    GC::Ref<JS::Error> ipc_error(JS::Realm& realm, Utf16View message, Utf16View code)
    {
        auto error = JS::Error::create(realm, message);
        error->define_direct_property(
            "code"_utf16, JS::PrimitiveString::create(realm.vm(), code), JS::default_attributes);
        return error;
    }
}

IpcEndpoint::IpcEndpoint(IsolatedWorld& world, Web::DOM::Document& document, String realm)
    : m_owner(world)
    , m_document(document)
    , m_realm(move(realm))
{
}

NonnullRefPtr<IpcEndpoint> IpcEndpoint::create(
    IsolatedWorld& world, Web::DOM::Document& document, String realm)
{
    auto endpoint = adopt_ref(*new IpcEndpoint(world, document, move(realm)));
    world.add_detach_hook([weak = endpoint->make_weak_ptr()] {
        if (auto owner = weak.strong_ref())
            owner->revoke();
    });
    return endpoint;
}

IpcEndpoint::~IpcEndpoint()
{
    revoke();
}

void IpcEndpoint::revoke()
{
    pending_count -= m_pending.size();
    m_pending.clear();
    m_listeners.clear();
    m_exports = nullptr;
    m_document = nullptr;
    m_owner = nullptr;
}

GC::Ref<JS::Object> IpcEndpoint::exports()
{
    if (m_exports)
        return *m_exports;
    auto& realm = *m_owner->realm();
    GC::Root result { JS::Object::create(realm, nullptr) };
    NonnullRefPtr owner { *this };
    GC::Root invoke { JS::NativeFunction::create(
        realm, [owner](JS::VM& vm) { return owner->invoke(vm); }, 1, "invoke"_utf16,
        GC::Ptr<JS::Realm> { realm }) };
    m_owner->block_function_export(*invoke);
    result->define_direct_property("invoke"_utf16, invoke, JS::default_attributes);
    GC::Root send { JS::NativeFunction::create(
        realm, [owner](JS::VM& vm) { return owner->invoke(vm, true); }, 1, "send"_utf16,
        GC::Ptr<JS::Realm> { realm }) };
    m_owner->block_function_export(*send);
    result->define_direct_property("send"_utf16, send, JS::default_attributes);
    // Electron 29+ drops the entire private module across contextBridge,
    // including event.sender. Individual raw native methods remain blocked.
    m_owner->redact_object_export(*result);
    m_exports = *result;
    auto method = [&](Utf16String name, ListenerOperation mode) {
        GC::Root function { JS::NativeFunction::create(
            realm, [owner, mode](JS::VM& vm) { return owner->listen(vm, mode); },
            mode == ListenerOperation::RemoveAll ? 1 : 2, name, GC::Ptr<JS::Realm> { realm }) };
        m_owner->block_function_export(*function);
        result->define_direct_property(name, function, JS::default_attributes);
    };

    method("on"_utf16, ListenerOperation::On);
    method("once"_utf16, ListenerOperation::Once);
    method("removeListener"_utf16, ListenerOperation::Remove);
    method("removeAllListeners"_utf16, ListenerOperation::RemoveAll);
    result->define_direct_property(
        "addListener"_utf16, result->get_without_side_effects("on"_utf16), JS::default_attributes);
    result->define_direct_property("off"_utf16,
        result->get_without_side_effects("removeListener"_utf16), JS::default_attributes);
    control("firefly.ipc.bind.v1"_string, 0);
    return *result;
}

JS::ThrowCompletionOr<JS::Value> IpcEndpoint::invoke(JS::VM& vm, bool event)
{
    auto& realm = *vm.current_realm();
    GC::Root<JS::PromiseCapability> capability;
    if (!event)
        capability = TRY(JS::new_promise_capability(vm, realm.intrinsics().promise_constructor()));
    auto reject = [&](Utf16View message, Utf16View code) -> JS::ThrowCompletionOr<JS::Value> {
        GC::Root error { ipc_error(realm, message, code) };
        if (event)
            return JS::throw_completion(error.ptr());
        TRY(JS::call(vm, *capability->reject(), JS::js_undefined(), error.ptr()));
        return capability->promise();
    };

    if (!m_owner || m_owner->is_detached() || !m_owner->is_executing() || m_owner->realm() != &realm
        || !m_document->is_fully_active())
        return reject(u"IPC document is no longer active"sv, u"ERR_FIREFLY_IPC_REVOKED"sv);
    if (!vm.argument(0).is_string()
        || vm.argument(0).as_string().utf16_string_view().length_in_code_units() > 256
        || vm.argument_count() > 65)
        return reject(
            u"IPC requires a string channel and at most 64 arguments"sv, u"ERR_INVALID_ARG_TYPE"sv);
    if (m_pending.size() >= 16 || pending_count >= 64 || m_next == 0xffffffffu)
        return reject(u"IPC pending request budget exceeded"sv, u"ERR_FIREFLY_IPC_LIMIT"sv);
    GC::Root args { TRY(JS::Array::create(realm, vm.argument_count())) };
    for (size_t i = 0; i < vm.argument_count(); ++i)
        args->define_direct_property(i, vm.argument(i), JS::default_attributes);
    auto captured = BridgeData::capture(JS::Value(args.ptr()), realm, {});
    if (captured.is_error())
        return reject(
            u"IPC arguments are not supported data"sv, u"ERR_FIREFLY_IPC_SERIALIZATION"sv);
    auto encoded = captured.value().encode_wire({});
    if (encoded.is_error() || encoded.value().length() > wire_limit)
        return reject(u"IPC data budget exceeded"sv, u"ERR_FIREFLY_IPC_SERIALIZATION"sv);
    auto* navigable = m_document->navigable().ptr();
    auto* page = navigable ? as_if<WebContent::PageClient>(navigable->page().client()) : nullptr;
    if (!page || !navigable->is_top_level_traversable())
        return reject(
            u"IPC requires the configured top-level document"sv, u"ERR_FIREFLY_IPC_REVOKED"sv);
    u32 id = ++m_next;
    m_pending.set(id,
        Pending {
            GC::Root { capability.ptr() }, MonotonicTime::now() + AK::Duration::from_seconds(30) });
    ++pending_count;
    JsonObject envelope;
    envelope.set("document"sv, document_key(*m_document));
    envelope.set("realm"sv, m_realm);
    envelope.set("request"sv, id);
    envelope.set("wire"sv, MUST(String::from_utf8(encoded.value().view())));
    // The event kind travels through the reserved native transport seam, never
    // as caller-supplied sender/permission metadata. The private ACK owns credit.
    page->webdriver_bidi_event(navigable->id(),
        event ? "firefly.ipc.send.v1"_string : "firefly.ipc.invoke.v1"_string, move(envelope));
    if (event)
        return JS::js_undefined();
    return capability->promise();
}

void IpcEndpoint::settle(u32 id, bool success, String const& wire)
{
    auto pending = m_pending.take(id);
    if (!pending.has_value())
        return;
    --pending_count;
    if (!pending->capability) {
        // send has no application-visible reply Promise. Report asynchronous
        // admission/timeout failures explicitly and release its private credit.
        if (!success)
            warnln("ERR_FIREFLY_IPC_SEND: asynchronous event admission/delivery failed");
        return;
    }

    if (!m_owner || m_owner->is_detached() || !m_document->is_fully_active())
        return;
    auto owner = m_owner;
    Web::HTML::TemporaryExecutionContext execution(m_document->relevant_settings_object());
    auto result = owner->run_async_frame([&]() -> JS::ThrowCompletionOr<JS::Value> {
        auto& realm = *owner->realm();
        auto& vm = realm.vm();
        auto decoded = wire.bytes().size() <= wire_limit
            ? BridgeData::decode_wire(wire.bytes_as_string_view(), {})
            : ErrorOr<BridgeData> { Error::from_string_literal("IPC reply too large") };
        JS::Value value;
        if (decoded.is_error()) {
            success = false;
            value = ipc_error(realm, u"Invalid IPC reply"sv, u"ERR_FIREFLY_IPC_SERIALIZATION"sv);
        } else {
            value
                = TRY(decoded.value().materialize(realm, {}, BridgeData::Materialization::Mutable));
            if (!success) {
                Utf16View message = u"IPC handler rejected"sv;
                if (value.is_string())
                    message = value.as_string().utf16_string_view();
                value = ipc_error(realm, message, u"ERR_FIREFLY_IPC_REMOTE"sv);
            }
        }

        auto function = success ? pending->capability->resolve() : pending->capability->reject();
        return JS::call(vm, *function, JS::js_undefined(), value);
    });
    if (result.is_error())
        warnln("Firefly IPC reply settlement rejected");
}

void IpcEndpoint::receive(WebContent::PageClient& page, Web::HTML::CrossProcessId navigable,
    JsonValue const& params, bool event)
{
    if (!m_document || !m_owner || m_owner->is_detached() || !m_document->is_fully_active()
        || !params.is_object())
        return;
    auto nav = m_document->navigable();
    if (!nav || nav->id() != navigable || &nav->page() != &page.page())
        return;
    auto const& data = params.as_object();
    if (data.size() != (event ? 4u : 5u))
        return;
    auto doc = data.get_string("document"sv), realm = data.get_string("realm"sv),
         wire = data.get_string("wire"sv);
    auto request = data.get_u32("request"sv);
    auto success = data.get_bool("success"sv);
    if (!doc.has_value() || *doc != document_key(*m_document) || !realm.has_value()
        || *realm != m_realm || !request.has_value() || !*request || !wire.has_value()
        || wire->bytes().size() > wire_limit || (!event && !success.has_value()))
        return;
    if (event) {
        if (*request <= m_incoming_last)
            return;
        m_incoming_last = *request;
        if (m_incoming_pending >= 16) {
            warnln("ERR_FIREFLY_IPC_SEND: renderer event queue exhausted");
            control("firefly.ipc.ack.v1"_string, *request);
            return;
        }

        ++m_incoming_pending;
        receive_event(*request, *wire);
    } else {
        settle(*request, *success, *wire);
    }
}

void IpcEndpoint::expire()
{
    Vector<u32> expired;
    auto now = MonotonicTime::now();
    for (auto const& entry : m_pending)
        if (entry.value.deadline <= now)
            expired.append(entry.key);
    if (expired.is_empty())
        return;
    if (!m_owner || m_owner->is_detached()) {
        revoke();
        return;
    }
    // Construct a native graph so timeout handling does not depend on wire bytes.
    auto encoded = MUST(BridgeData::from_string(u"IPC request timed out"sv).encode_wire({}));
    auto wire = MUST(String::from_utf8(encoded.view()));
    for (auto id : expired)
        settle(id, false, wire);
}
}
