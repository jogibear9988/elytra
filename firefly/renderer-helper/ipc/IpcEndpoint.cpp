/** Preload-only invoke/event endpoint. Own Promise roots or private event credit until
 * settlement, timeout or document revocation; accept bounded data graphs only. */
#include "IpcEndpoint.h"
#include "ports/PortEndpoint.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include <AK/JsonObject.h>
#include <AK/NeverDestroyed.h>
#include <LibCore/EventLoop.h>
#include <LibGC/Function.h>
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
void IpcEndpoint::visit_edges(GC::Cell::Visitor& visitor) const
{
    visitor.visit(m_document.ptr());
    visitor.visit(m_exports.ptr());
    if (m_ports)
        m_ports->visit_edges(visitor);
    for (auto const& listener : m_listeners)
        visitor.visit(listener.callback.ptr());
    for (auto const& entry : m_pending)
        visitor.visit(entry.value.capability.ptr());
}

namespace {
    constexpr size_t wire_limit = ApplicationIpc::logical_wire_limit;
    size_t pending_count;
    size_t queued_count;
    NeverDestroyed<Vector<WeakPtr<IpcEndpoint>>> endpoints;

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
    m_ports = make<PortEndpoint>(*this);
}

NonnullRefPtr<IpcEndpoint> IpcEndpoint::create(
    IsolatedWorld& world, Web::DOM::Document& document, String realm)
{
    auto endpoint = adopt_ref(*new IpcEndpoint(world, document, move(realm)));
    endpoints->append(endpoint->make_weak_ptr());
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
    // Revoke before closing: a synchronous close callback must not settle work
    // or reenter a world while its document is being detached.
    m_byte_failed = true;
    if (m_ports)
        m_ports->revoke();
    if (m_byte_channel) {
        m_byte_channel->close();
        m_byte_channel = nullptr;
    }

    clear_packets();
    queued_count -= m_calls.size();
    m_calls.clear();
    pending_count -= m_active_count;
    m_active_count = 0;
    m_admission_scheduled = false;
    m_pending.clear();
    // A destructor can enter here after its RefCount reached zero, before the
    // Weakable base clears its link. Do not strong-ref that retiring endpoint.
    endpoints->remove_all_matching(
        [this](auto const& weak) { return !weak || weak.ptr() == this; });
    wake_admission();
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
    GC::Root invoke { JS::NativeFunction::create(realm,
        GC::create_function((realm).vm().heap(), GC::captures(owner),
            [](auto& owner, JS::VM& vm) { return owner->invoke(vm); }),
        1, "invoke"_utf16, GC::Ptr<JS::Realm> { realm }) };
    m_owner->block_function_export(*invoke);
    result->define_direct_property("invoke"_utf16, invoke, JS::default_attributes);
    GC::Root send { JS::NativeFunction::create(realm,
        GC::create_function((realm).vm().heap(), GC::captures(owner),
            [](auto& owner, JS::VM& vm) { return owner->invoke(vm, true); }),
        1, "send"_utf16, GC::Ptr<JS::Realm> { realm }) };
    m_owner->block_function_export(*send);
    result->define_direct_property("send"_utf16, send, JS::default_attributes);
    // Electron 29+ drops the entire private module across contextBridge,
    // including event.sender. Individual raw native methods remain blocked.
    m_owner->redact_object_export(*result);
    m_exports = *result;
    auto method = [&](Utf16String name, ListenerOperation mode) {
        GC::Root function { JS::NativeFunction::create(realm,
            GC::create_function((realm).vm().heap(), GC::captures(owner, mode),
                [](auto& owner, auto& mode, JS::VM& vm) { return owner->listen(vm, mode); }),
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

    if (m_byte_failed || !m_owner || m_owner->is_detached() || !m_owner->is_executing()
        || m_owner->realm() != &realm || !m_document->is_fully_active())
        return reject(u"IPC document is no longer active"sv, u"ERR_FIREFLY_IPC_REVOKED"sv);
    if (!vm.argument(0).is_string()
        || vm.argument(0).as_string().utf16_string_view().length_in_code_units() > 256
        || vm.argument_count() > 65)
        return reject(
            u"IPC requires a string channel and at most 64 arguments"sv, u"ERR_INVALID_ARG_TYPE"sv);
    if (m_calls.size() >= 256 || queued_count >= 1024 || m_next == 0xffffffffu)
        return reject(u"IPC admission queue budget exceeded"sv, u"ERR_FIREFLY_IPC_LIMIT"sv);
    GC::Root args { TRY(JS::Array::create(realm, vm.argument_count())) };
    for (size_t i = 0; i < vm.argument_count(); ++i)
        args->define_direct_property(i, vm.argument(i), JS::default_attributes);
    auto captured = BridgeData::capture_ipc(JS::Value(args.ptr()), realm, application_ipc_limits());
    if (captured.is_error())
        return reject(
            u"IPC arguments are not supported data"sv, u"ERR_FIREFLY_IPC_SERIALIZATION"sv);
    auto encoded = captured.value().encode_wire(application_ipc_limits());
    if (encoded.is_error() || encoded.value().length() > wire_limit)
        return reject(u"IPC data budget exceeded"sv, u"ERR_FIREFLY_IPC_SERIALIZATION"sv);
    auto* navigable = m_document->navigable().ptr();
    auto* page = navigable ? as_if<WebContent::PageClient>(navigable->page().client()) : nullptr;
    if (!page || !navigable->is_top_level_traversable())
        return reject(
            u"IPC requires the configured top-level document"sv, u"ERR_FIREFLY_IPC_REVOKED"sv);
    auto reservation = m_wire_budget->reserve(encoded.value().length());
    if (!reservation)
        return reject(u"IPC admission wire budget exceeded"sv, u"ERR_FIREFLY_IPC_LIMIT"sv);
    u32 id = ++m_next;
    m_pending.set(id,
        Pending {
            GC::Root { capability.ptr() }, MonotonicTime::now() + AK::Duration::from_seconds(30) });
    ++queued_count;
    m_calls.append({ event ? ApplicationIpc::Kind::Send : ApplicationIpc::Kind::Invoke, id,
        encoded.release_value(), move(reservation) });
    schedule_admission();

    if (event)
        return JS::js_undefined();
    return capability->promise();
}

void IpcEndpoint::settle(
    u32 id, bool success, ByteString const& wire, RefPtr<IpcWireReservation> reservation)
{
    auto found = m_pending.find(id);
    if (found == m_pending.end() || found->value.settling)
        return;
    if (m_owner && m_owner->is_executing()) {
        found->value.settling = true;
        Core::deferred_invoke(
            [self = NonnullRefPtr { *this }, id, success, wire, reservation = move(reservation)] {
                if (auto found = self->m_pending.find(id); found != self->m_pending.end())
                    found->value.settling = false;
                self->settle(id, success, wire, reservation);
            });
        return;
    }

    auto pending = m_pending.take(id);
    if (!pending.has_value())
        return;
    if (pending->admitted) {
        --pending_count;
        --m_active_count;
    } else {
        for (size_t i = 0; i < m_calls.size(); ++i) {
            if (m_calls[i].request == id) {
                m_calls.remove(i);
                --queued_count;
                break;
            }
        }
    }

    ScopeGuard wake = [] { wake_admission(); };
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
        if (m_byte_failed)
            success = false;
        auto decoded = m_byte_failed
            ? ErrorOr<BridgeData> { BridgeData::from_string(u"IPC document channel closed"sv) }
            : wire.length() <= wire_limit
            ? BridgeData::decode_wire(wire.view(), application_ipc_limits())
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

void IpcEndpoint::wake_admission()
{
    endpoints->remove_all_matching([](auto const& weak) { return !weak; });
    for (auto const& weak : *endpoints)
        if (auto owner = weak.strong_ref())
            owner->schedule_admission();
}

void IpcEndpoint::schedule_admission()
{
    if (m_admission_scheduled || m_byte_failed || m_calls.is_empty() || m_active_count >= 16
        || pending_count >= 64 || !m_byte_channel || !m_byte_channel->is_open()
        || m_byte_queue.size() >= ApplicationIpc::queue_limit)
        return;
    m_admission_scheduled = true;
    Core::deferred_invoke([weak = make_weak_ptr()] {
        if (auto owner = weak.strong_ref()) {
            owner->m_admission_scheduled = false;
            owner->pump_admission();
        }
    });
}

void IpcEndpoint::pump_admission()
{
    if (m_byte_failed || !m_owner || m_owner->is_detached() || !m_document
        || !m_document->is_fully_active() || m_calls.is_empty() || m_active_count >= 16
        || pending_count >= 64 || !m_byte_channel || !m_byte_channel->is_open()
        || m_byte_queue.size() >= ApplicationIpc::queue_limit)
        return;
    auto found = m_pending.find(m_calls.first().request);
    VERIFY(found != m_pending.end());
    if (found->value.settling)
        return;
    if (found->value.deadline <= MonotonicTime::now()) {
        auto error = MUST(BridgeData::from_string(u"IPC request timed out"sv).encode_wire({}));
        settle(found->key, false, error);
        return;
    }

    auto call = m_calls.take_first();
    --queued_count;
    found->value.admitted = true;
    ++m_active_count;
    ++pending_count;
    // Send both halves only when native admission credit is available. Reserving
    // the encoded snapshot at call time prevents mutation and double charging.
    control(call.kind == ApplicationIpc::Kind::Send ? "firefly.ipc.send.v1"_string
                                                    : "firefly.ipc.invoke.v1"_string,
        call.request);
    m_byte_queue.append(
        { call.kind, call.request, false, move(call.wire), 0, move(call.reservation) });
    flush_packets();
    // One request per deferred turn gives other document endpoints a chance at
    // the renderer-wide active credit; physical fragments retain their own FIFO.
    schedule_admission();
}

void IpcEndpoint::expire()
{
    if (m_ports)
        m_ports->expire();
    auto now = MonotonicTime::now();
    if (m_fragment_deadline.has_value() && *m_fragment_deadline <= now) {
        retire_channel();
        return;
    }

    Vector<u32> expired;
    for (auto const& entry : m_pending) {
        // An authenticated reply already received while source code was busy
        // wins over its deadline; its deferred settlement owns the first result.
        if (entry.value.settling || entry.value.deadline > now)
            continue;
        // A published request may still occupy main admission or a partial
        // fragment. Never recycle its credit while that peer state is unknown.
        if (entry.value.admitted) {
            retire_channel();
            return;
        }

        expired.append(entry.key);
    }

    if (expired.is_empty())
        return;
    if (!m_owner || m_owner->is_detached()) {
        revoke();
        return;
    }
    // Construct a native graph so timeout handling does not depend on wire bytes.
    auto encoded = MUST(BridgeData::from_string(u"IPC request timed out"sv).encode_wire({}));
    auto wire = encoded;
    for (auto id : expired)
        settle(id, false, wire);
}
}
