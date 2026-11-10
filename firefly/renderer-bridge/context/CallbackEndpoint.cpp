/** Copy calls back to the principal world in a clean frame, with guarded Promise observation. */
#include "Boundary.h"
#include "CallbackBridge.h"
#include "Snapshot.h"
#include "serialization/BridgeData.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Function.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Runtime/PromiseConstructor.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Window.h>

namespace Firefly {
CallbackEndpoint::CallbackEndpoint(CallbackRegistry& registry, JS::FunctionObject& function)
    : m_registry(registry)
    , m_function(function)
{
}

void CallbackEndpoint::visit_edges(GC::Cell::Visitor& visitor) const
{
    visitor.visit(m_function.ptr());
    if (m_registry)
        visitor.visit(m_registry->document.ptr());
    for (auto const& entry : m_pending) {
        visitor.visit(entry.value.capability.ptr());
        visitor.visit(entry.value.target.ptr());
    }
}

CallbackEndpoint::~CallbackEndpoint()
{
    // NativeFunction closures can be finalized by GC. Release counters/roots
    // without allocating errors or executing Promise settlement during collection.
    if (m_registry) {
        VERIFY(m_registry->pending >= m_pending.size());
        m_registry->pending -= m_pending.size();
    }
}

ErrorOr<CallbackExport> CallbackEndpoint::create(
    CallbackRegistry& registry, JS::FunctionObject& function)
{
    auto owner = adopt_ref(*new CallbackEndpoint(registry, function));
    auto& target = *registry.world->realm();
    GC::Root proxy { JS::NativeFunction::create(target,
        GC::create_function((target).vm().heap(), GC::captures(owner),
            [](auto& owner, JS::VM& vm) { return owner->invoke(vm); }),
        0, "fireflyBridgeCallback"_utf16, GC::Ptr<JS::Realm> { target }) };
    auto frozen = proxy->set_integrity_level(JS::Object::IntegrityLevel::Frozen);
    if (frozen.is_error() || !frozen.value()) {
        owner->revoke();
        return Error::from_string_literal("Cannot freeze callback proxy");
    }

    return CallbackExport { *proxy, owner };
}

void CallbackEndpoint::revoke()
{
    if (!m_registry)
        return;
    auto registry = move(m_registry);
    m_function = nullptr;
    auto pending = move(m_pending);
    for (auto& entry : pending) {
        VERIFY(registry->pending);
        --registry->pending;
        Boundary::reject(*entry.value.capability, *entry.value.target);
    }
}

JS::ThrowCompletionOr<JS::Value> CallbackEndpoint::invoke(JS::VM& vm)
{
    auto error = [&] {
        return JS::throw_completion(
            Boundary::error(*vm.current_realm(), u"Firefly bridge callback rejected"sv));
    };

    if (!m_registry || !m_registry->active()) {
        revoke();
        return error();
    }

    auto registry = m_registry;
    auto world = registry->world;
    GC::Root document { *registry->document };
    GC::Root target { *world->realm() };
    GC::Root source { document->window()->principal_realm() };
    GC::Root function { *m_function };
    if (vm.current_realm() != target.ptr() || !world->is_executing()
        || !Boundary::caller_is(vm, *target) || vm.argument_count() > 64
        || registry->principal_depth >= 32)
        return error();
    GC::RootVector<JS::Value> arguments;
    for (size_t i = 0; i < vm.argument_count(); ++i)
        arguments.append(vm.argument(i));
    GC::Root array { JS::Array::create_from(*target, arguments.span()) };
    auto data = capture_to_principal(*world, JS::Value(array.ptr()));
    if (data.is_error())
        return error();
    auto copied
        = data.value().materialize(*source, {}, BridgeData::Materialization::FrozenContainers);
    if (copied.is_error())
        return error();
    auto root = GC::Root<JS::Value>::create(copied.value(), SourceLocation::current());
    GC::RootVector<JS::Value> params;
    for (size_t i = 0; i < arguments.size(); ++i)
        params.append(copied.value().as_object().get_without_side_effects(i));
    ++registry->principal_depth;
    ScopeGuard depth([&] { --registry->principal_depth; });
    auto result = Boundary::in_realm(
        *source, [&] { return JS::call(vm, *function, JS::js_undefined(), params.span()); });
    if (!m_registry || !registry->active() || world->is_detached() || result.is_error())
        return error();
    auto value = result.release_value();
    auto result_root = GC::Root<JS::Value>::create(value, SourceLocation::current());
    if (value.is_object() && value.as_object().engine_class_id() == JS_LAYOUT_CLASS_ID_PROMISE
        && !JS::host_class_of(value.as_object())
        && &value.as_object().shape().realm() == source.ptr())
        return observe(static_cast<JS::Promise&>(value.as_object()), *target);
    auto snapshot = BridgeData::capture(value, *source, public_binary_limits());
    if (snapshot.is_error())
        return error();
    auto answer
        = snapshot.value().materialize(*target, {}, BridgeData::Materialization::FrozenContainers);
    if (answer.is_error())
        return error();
    return answer.release_value();
}

JS::ThrowCompletionOr<JS::Value> CallbackEndpoint::observe(JS::Promise& promise, JS::Realm& target)
{
    auto& vm = target.vm();
    GC::Root source { promise };
    if (!m_registry || !m_registry->active() || m_registry->pending >= 64
        || m_next == NumericLimits<u64>::max())
        return JS::throw_completion(Boundary::error(target, u"Firefly bridge callback rejected"sv));
    GC::Root capability { TRY(
        JS::new_promise_capability(vm, target.intrinsics().promise_constructor())) };
    if (!m_registry || !m_registry->active() || m_registry->pending >= 64)
        return JS::throw_completion(Boundary::error(target, u"Firefly bridge callback rejected"sv));
    ++m_registry->pending;
    auto id = ++m_next;
    m_pending.set(id, { GC::Root { *capability }, GC::Root { target } });
    NonnullRefPtr self { *this };
    auto& principal = m_registry->document->window()->principal_realm();
    GC::Root fulfilled { JS::NativeFunction::create(principal,
        GC::create_function((principal).vm().heap(), GC::captures(self, id),
            [](auto& self, auto& id, JS::VM& inner) {
                return self->settle(id, true, inner.argument(0));
            }),
        1, "fireflyCallbackFulfilled"_utf16, GC::Ptr<JS::Realm> { principal }) };
    GC::Root rejected { JS::NativeFunction::create(principal,
        GC::create_function((principal).vm().heap(), GC::captures(self, id),
            [](auto& self, auto& id, JS::VM&) {
                return self->settle(id, false, JS::js_undefined());
            }),
        1, "fireflyCallbackRejected"_utf16, GC::Ptr<JS::Realm> { principal }) };
    // Already-settled principal promises must not inherit a source cancellation frame.
    TRY(Boundary::in_realm(principal, [&]() -> JS::ThrowCompletionOr<JS::Value> {
        promise.perform_then(fulfilled.ptr(), rejected.ptr(), nullptr);
        return JS::js_undefined();
    }));
    return capability->promise();
}

JS::ThrowCompletionOr<JS::Value> CallbackEndpoint::settle(u64 id, bool fulfilled, JS::Value value)
{
    NonnullRefPtr protect { *this };
    auto pending = m_pending.take(id);
    if (!pending.has_value())
        return JS::js_undefined();
    VERIFY(m_registry && m_registry->pending);
    --m_registry->pending;
    auto record = pending.release_value();
    if (!m_registry->active() || !fulfilled) {
        Boundary::reject(*record.capability, *record.target);
        return JS::js_undefined();
    }

    auto& source = m_registry->document->window()->principal_realm();
    auto data = BridgeData::capture(value, source, public_binary_limits());
    if (data.is_error()) {
        Boundary::reject(*record.capability, *record.target);
        return JS::js_undefined();
    }

    auto copied = data.value().materialize(
        *record.target, {}, BridgeData::Materialization::FrozenContainers);
    if (copied.is_error() || !Boundary::resolve_safe(copied.value()) || !m_registry->active()) {
        Boundary::reject(*record.capability, *record.target);
        return JS::js_undefined();
    }

    Boundary::settle(*record.capability, *record.target, copied.release_value(), false);
    return JS::js_undefined();
}
}
