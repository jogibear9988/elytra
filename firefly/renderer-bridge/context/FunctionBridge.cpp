/**
 * Publishes revocable cross-world function endpoints and observes Promise results. Explicit
 * pending roots must be released before their source document/world is torn down.
 */

#include "context/FunctionBridge.h"
#include "context/Boundary.h"
#include "context/CallbackBridge.h"
#include "context/Snapshot.h"
#include "serialization/BridgeData.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Function.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Runtime/PromiseCapability.h>
#include <LibJS/Runtime/PromiseConstructor.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Window.h>

namespace Firefly {
struct FunctionBridge::Pending : public RefCounted<Pending> {
    GC::Root<JS::PromiseCapability> capability;
    GC::Root<JS::Realm> target;

    Pending(JS::PromiseCapability& capability, JS::Realm& target)
        : capability(capability)
        , target(target)
    {
    }
};

void FunctionBridge::visit_edges(GC::Cell::Visitor& visitor) const
{
    // Native ownership/revocation keeps the existing root lifetime. Explicit
    // captures expose these direct dependencies without strengthening weak caches.
    visitor.visit(m_function.ptr());
    visitor.visit(m_document.ptr());
    for (auto const& entry : m_pending) {
        visitor.visit(entry.value->capability.ptr());
        visitor.visit(entry.value->target.ptr());
    }
}

FunctionBridge::~FunctionBridge() = default;

FunctionBridge::FunctionBridge(IsolatedWorld& source, JS::FunctionObject& function,
    Web::DOM::Document& document, CallbackRegistry& callbacks, bool own_registry)
    : m_source(source)
    , m_function(function)
    , m_document(document)
    , m_callbacks(callbacks)
    , m_own_registry(own_registry)
{
}

ErrorOr<FunctionExport> FunctionBridge::create(IsolatedWorld& source, JS::FunctionObject& function,
    Web::DOM::Document& document, RefPtr<CallbackRegistry> callbacks)
{
    // FunctionObject::realm() is a native field read. Callable proxies/bound
    // functions have no own realm and are rejected without invoking their hooks.
    if (source.is_detached() || source.function_export_blocked(function)
        || function.realm() != source.realm() || !document.is_fully_active()
        || Web::Bindings::host_defined_of(*source.realm()).principal_realm.ptr()
            != &document.window()->principal_realm())
        return Error::from_string_literal(
            "Function export requires its live source world and exact document");
    bool own_registry = !callbacks;
    if (!callbacks)
        callbacks = TRY(CallbackRegistry::create(source, document));
    auto owner
        = adopt_ref(*new FunctionBridge(source, function, document, *callbacks, own_registry));
    auto& target = document.window()->principal_realm();
    // The first create parameter selects a VM for allocation, not [[Realm]].
    // Set the optional realm explicitly to keep constructor/error paths in target.
    GC::Root proxy { JS::NativeFunction::create(target,
        GC::create_function((target).vm().heap(), GC::captures(owner),
            [](auto& owner, JS::VM& vm) { return owner->invoke(vm); }),
        0, "fireflyBridgeFunction"_utf16, GC::Ptr<JS::Realm> { target }) };
    if (proxy->realm() != &target) {
        owner->revoke();
        return Error::from_string_literal("Exported function has the wrong realm");
    }

    auto frozen = proxy->set_integrity_level(JS::Object::IntegrityLevel::Frozen);
    if (frozen.is_error() || !frozen.value()) {
        owner->revoke();
        return Error::from_string_literal("Cannot freeze exported function");
    }

    return FunctionExport { owner, *proxy };
}

void FunctionBridge::revoke()
{
    m_revoked = true;
    m_function = nullptr;
    m_document = nullptr;
    m_source.clear();
    if (m_callbacks && m_own_registry)
        m_callbacks->revoke();
    m_callbacks.clear();
    auto pending = move(m_pending);
    for (auto& entry : pending)
        Boundary::reject(*entry.value->capability, *entry.value->target);
}

JS::ThrowCompletionOr<JS::Value> FunctionBridge::observe(
    JS::VM& vm, JS::Promise& promise, JS::Realm& target)
{
    GC::Root source_promise { promise };
    if (m_pending.size() >= 64 || m_next_id == NumericLimits<u64>::max())
        return JS::throw_completion(
            Boundary::error(*vm.current_realm(), u"Firefly bridge call rejected"sv));
    GC::Root capability { TRY(
        JS::new_promise_capability(vm, target.intrinsics().promise_constructor())) };
    u64 id = ++m_next_id;
    m_pending.set(id, adopt_ref(*new Pending(*capability, target)));
    NonnullRefPtr owner { *this };
    // Observe engine state directly, bypassing .then, constructor and species.
    // Reactions capture only owner/id, so revoke can release the owned pending roots.
    GC::Root fulfilled { JS::NativeFunction::create(target,
        GC::create_function((target).vm().heap(), GC::captures(owner, id),
            [](auto& owner, auto& id, JS::VM& inner) {
                return owner->settle(id, true, inner.argument(0));
            }),
        1, "fireflyPromiseFulfilled"_utf16, GC::Ptr<JS::Realm> { target }) };
    GC::Root rejected { JS::NativeFunction::create(target,
        GC::create_function((target).vm().heap(), GC::captures(owner, id),
            [](auto& owner, auto& id, JS::VM&) {
                return owner->settle(id, false, JS::js_undefined());
            }),
        1, "fireflyPromiseRejected"_utf16, GC::Ptr<JS::Realm> { target }) };
    promise.perform_then(fulfilled.ptr(), rejected.ptr(), nullptr);
    return capability->promise();
}

JS::ThrowCompletionOr<JS::Value> FunctionBridge::settle(u64 id, bool fulfilled, JS::Value value)
{
    NonnullRefPtr protect { *this };
    auto pending = m_pending.take(id);
    if (!pending.has_value())
        return JS::js_undefined(); // late/duplicate completion
    auto& record = **pending;
    GC::Root capability { *record.capability };
    GC::Root target { *record.target };
    record.capability = nullptr;
    record.target = nullptr;
    if (m_revoked || m_source->is_detached() || !m_document->is_fully_active()) {
        revoke();
        Boundary::reject(*capability, *target);
        return JS::js_undefined();
    }

    if (!fulfilled) {
        Boundary::reject(*capability, *target);
        return JS::js_undefined();
    }

    auto data = capture_to_principal(*m_source, value);
    if (data.is_error()) {
        Boundary::reject(*capability, *target);
        return JS::js_undefined();
    }

    auto copied
        = data.value().materialize(*target, {}, BridgeData::Materialization::FrozenContainers);
    if (copied.is_error() || !Boundary::resolve_safe(copied.value()) || m_revoked
        || m_source->is_detached() || !m_document->is_fully_active()) {
        if (!m_revoked && (m_source->is_detached() || !m_document->is_fully_active()))
            revoke();
        Boundary::reject(*capability, *target);
        return JS::js_undefined();
    }

    Boundary::settle(*capability, *target, copied.release_value(), false);
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> FunctionBridge::invoke(JS::VM& vm)
{
    auto error = [&]() {
        return JS::throw_completion(
            Boundary::error(*vm.current_realm(), u"Firefly bridge call rejected"sv));
    };

    if (m_revoked)
        return error();
    if (!m_document->is_fully_active() || m_source->is_detached()) {
        revoke();
        return error();
    }

    if (m_call_depth >= 32 || m_pending.size() >= 64 || vm.argument_count() > 64
        || vm.current_realm() != &m_document->window()->principal_realm()
        || !Boundary::caller_is(vm, m_document->window()->principal_realm())
        || (m_source->is_executing() && !m_callbacks->principal_depth))
        return error();
    NonnullRefPtr protect { *this };
    NonnullRefPtr source { *m_source };
    GC::Root function { *m_function };
    GC::Root target { *vm.current_realm() };
    GC::Root source_realm { *source->realm() };
    GC::Root document { *m_document };
    auto callbacks = m_callbacks;
    ++m_call_depth;
    ScopeGuard done([&] { --m_call_depth; });
    GC::RootVector<JS::Value> arguments;
    for (size_t i = 0; i < vm.argument_count(); ++i)
        arguments.append(vm.argument(i));
    GC::Root array { JS::Array::create_from(*target, arguments.span()) };
    GC::RootVector<JS::Value> functions;
    Vector<RefPtr<CallbackEndpoint>> created;
    bool committed = false;
    ScopeGuard rollback([&] {
        if (!committed)
            for (auto& callback : created)
                callbacks->discard(*callback);
    });
    auto data = BridgeData::capture(JS::Value(array.ptr()), *target, public_binary_limits(),
        [&](JS::FunctionObject& function) -> ErrorOr<size_t> {
            auto exported = TRY(callbacks->proxy(function));
            auto id = functions.size();
            functions.append(exported.function);
            if (exported.created)
                created.append(move(exported.created));
            return id;
        });
    if (data.is_error())
        return error();
    auto copied = data.value().materialize(
        *source_realm,
        [&](size_t index) -> JS::ThrowCompletionOr<JS::Value> {
            VERIFY(index < functions.size());
            return functions[index];
        },
        BridgeData::Materialization::FrozenContainers);
    if (copied.is_error())
        return error();
    auto copied_root = GC::Root<JS::Value>::create(copied.value(), SourceLocation::current());
    GC::RootVector<JS::Value> source_arguments;
    for (size_t i = 0; i < arguments.size(); ++i)
        source_arguments.append(copied.value().as_object().get_without_side_effects(i));
    // Application code may retain callbacks before throwing. Commit ownership
    // at argument delivery, not after its result is known.
    committed = true;
    auto result = source->is_executing()
        ? source->call_from_callback(*function, source_arguments.span())
        : source->call(*function, source_arguments.span());
    // Never inspect or stringify an exception supplied by the source world.
    // Recheck revocation after the source call, including native reentrant hooks.
    if (m_revoked || source->is_detached() || !document->is_fully_active()) {
        revoke();
        return error();
    }

    if (result.is_error())
        return error();
    auto result_value = result.release_value();
    if (result_value.is_object()) {
        auto& object = result_value.as_object();
        if (object.engine_class_id() == JS_LAYOUT_CLASS_ID_PROMISE && !JS::host_class_of(object)
            && &object.shape().realm() == source_realm.ptr())
            return observe(vm, static_cast<JS::Promise&>(object), *target);
    }

    auto output = capture_to_principal(*source, result_value);
    if (output.is_error())
        return error();
    auto answer
        = output.value().materialize(*target, {}, BridgeData::Materialization::FrozenContainers);
    if (answer.is_error())
        return error();
    return answer.release_value();
}
}
