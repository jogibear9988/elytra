/**
 * Publishes revocable cross-world function endpoints and observes Promise results. Explicit
 * pending roots must be released before their source document/world is torn down.
 */

#include "context/FunctionBridge.h"
#include "serialization/BridgeData.h"
#include <AK/ScopeGuard.h>
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

FunctionBridge::~FunctionBridge() = default;

static GC::Ref<JS::TypeError> bridge_error(JS::Realm& target, Utf16View message)
{
    auto& vm = target.vm();
    // Capture no caller/source frames, including revoke during a source call.
    vm.save_execution_context_stack();
    ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
    auto context = JS::ExecutionContext::create(0, ReadonlySpan<JS::Value> {}, 0);
    context->realm = &target;
    vm.push_execution_context(*context);
    ScopeGuard pop([&] { VERIFY(vm.pop_execution_context() == context.ptr()); });
    return JS::TypeError::create(target, message);
}

static void settle_target(
    JS::PromiseCapability& capability, JS::Realm& target, JS::Value value, bool reject)
{
    auto& vm = target.vm();
    auto root = GC::Root<JS::Value>::create(value, SourceLocation::current());
    // Approved copied values and sanitized errors belong to target continuations,
    // not to the source job's cancellation generation. Never inherit its frames
    // when built-in settlement enqueues the target's Promise reactions.
    vm.save_execution_context_stack();
    ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
    auto context = JS::ExecutionContext::create(0, ReadonlySpan<JS::Value> {}, 0);
    context->realm = &target;
    vm.push_execution_context(*context);
    ScopeGuard pop([&] { VERIFY(vm.pop_execution_context() == context.ptr()); });
    auto function = reject ? capability.reject() : capability.resolve();
    MUST(JS::call(vm, *function, JS::js_undefined(), value));
}

static void reject_pending(JS::PromiseCapability& capability, JS::Realm& target)
{
    GC::Root error { bridge_error(target, u"Firefly bridge promise rejected"sv) };
    settle_target(capability, target, error.ptr(), true);
}

FunctionBridge::FunctionBridge(
    IsolatedWorld& source, JS::FunctionObject& function, Web::DOM::Document& document)
    : m_source(source)
    , m_function(function)
    , m_document(document)
{
}

ErrorOr<FunctionExport> FunctionBridge::create(
    IsolatedWorld& source, JS::FunctionObject& function, Web::DOM::Document& document)
{
    // FunctionObject::realm() is a native field read. Callable proxies/bound
    // functions have no own realm and are rejected without invoking their hooks.
    if (source.is_detached() || function.realm() != source.realm() || !document.is_fully_active()
        || Web::Bindings::host_defined_of(*source.realm()).principal_realm.ptr()
            != &document.window()->principal_realm())
        return Error::from_string_literal(
            "Function export requires its live source world and exact document");
    auto owner = adopt_ref(*new FunctionBridge(source, function, document));
    auto& target = document.window()->principal_realm();
    // The first create parameter selects a VM for allocation, not [[Realm]].
    // Set the optional realm explicitly to keep constructor/error paths in target.
    GC::Root proxy { JS::NativeFunction::create(
        target, [owner](JS::VM& vm) { return owner->invoke(vm); }, 0, "fireflyBridgeFunction"_utf16,
        GC::Ptr<JS::Realm> { target }) };
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
    auto pending = move(m_pending);
    for (auto& entry : pending)
        reject_pending(*entry.value->capability, *entry.value->target);
}

JS::ThrowCompletionOr<JS::Value> FunctionBridge::observe(
    JS::VM& vm, JS::Promise& promise, JS::Realm& target)
{
    GC::Root source_promise { promise };
    if (m_pending.size() >= 64 || m_next_id == NumericLimits<u64>::max())
        return JS::throw_completion(
            bridge_error(*vm.current_realm(), u"Firefly bridge call rejected"sv));
    GC::Root capability { TRY(
        JS::new_promise_capability(vm, target.intrinsics().promise_constructor())) };
    u64 id = ++m_next_id;
    m_pending.set(id, adopt_ref(*new Pending(*capability, target)));
    NonnullRefPtr owner { *this };
    // Observe engine state directly, bypassing .then, constructor and species.
    // Reactions capture only owner/id, so revoke can release the owned pending roots.
    GC::Root fulfilled { JS::NativeFunction::create(
        target, [owner, id](JS::VM& inner) { return owner->settle(id, true, inner.argument(0)); },
        1, "fireflyPromiseFulfilled"_utf16, GC::Ptr<JS::Realm> { target }) };
    GC::Root rejected { JS::NativeFunction::create(
        target, [owner, id](JS::VM&) { return owner->settle(id, false, JS::js_undefined()); }, 1,
        "fireflyPromiseRejected"_utf16, GC::Ptr<JS::Realm> { target }) };
    promise.perform_then(fulfilled.ptr(), rejected.ptr(), nullptr);
    return capability->promise();
}

// Promise resolving functions perform Get(value, "then"). Refuse a polluted
// target prototype before that publication point, so no caller getter/thenable
// can steal a still-pending settlement from the revocation registry.
static bool safe_to_resolve(JS::Value value)
{
    if (!value.is_object())
        return true;
    auto const* object = &value.as_object();
    for (size_t depth = 0; object && depth < 64; ++depth, object = object->prototype()) {
        auto id = object->engine_class_id();
        if (JS::host_class_of(*object)
            || (id != JS_LAYOUT_CLASS_ID_OBJECT && id != JS_LAYOUT_CLASS_ID_ARRAY
                && id != JS_LAYOUT_CLASS_ID_OBJECT_PROTOTYPE
                && id != JS_LAYOUT_CLASS_ID_ARRAY_PROTOTYPE))
            return false;
        auto descriptor = object->ordinary_get_own_property("then"_utf16);
        if (descriptor.is_error())
            return false;
        if (descriptor.value().has_value())
            return descriptor.value()->is_data_descriptor() && descriptor.value()->value.has_value()
                && !descriptor.value()->value->is_function();
    }

    return !object;
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
        reject_pending(*capability, *target);
        return JS::js_undefined();
    }

    if (!fulfilled) {
        reject_pending(*capability, *target);
        return JS::js_undefined();
    }

    auto data = BridgeData::capture(value, *m_source->realm(), {});
    if (data.is_error()) {
        reject_pending(*capability, *target);
        return JS::js_undefined();
    }

    auto copied = data.value().materialize(*target);
    if (copied.is_error() || !safe_to_resolve(copied.value()) || m_revoked
        || m_source->is_detached() || !m_document->is_fully_active()) {
        if (!m_revoked && (m_source->is_detached() || !m_document->is_fully_active()))
            revoke();
        reject_pending(*capability, *target);
        return JS::js_undefined();
    }

    settle_target(*capability, *target, copied.release_value(), false);
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> FunctionBridge::invoke(JS::VM& vm)
{
    auto error = [&]() {
        return JS::throw_completion(
            bridge_error(*vm.current_realm(), u"Firefly bridge call rejected"sv));
    };

    if (m_revoked)
        return error();
    if (!m_document->is_fully_active() || m_source->is_detached()) {
        revoke();
        return error();
    }

    if (m_calling || m_pending.size() >= 64 || vm.argument_count() > 64
        || vm.current_realm() != &m_document->window()->principal_realm())
        return error();
    NonnullRefPtr protect { *this };
    NonnullRefPtr source { *m_source };
    GC::Root function { *m_function };
    GC::Root target { *vm.current_realm() };
    GC::Root source_realm { *source->realm() };
    GC::Root document { *m_document };
    m_calling = true;
    ScopeGuard done([&] { m_calling = false; });
    GC::RootVector<JS::Value> arguments;
    for (size_t i = 0; i < vm.argument_count(); ++i)
        arguments.append(vm.argument(i));
    GC::Root array { JS::Array::create_from(*target, arguments.span()) };
    auto data = BridgeData::capture(JS::Value(array.ptr()), *target, {});
    if (data.is_error())
        return error();
    auto copied = data.value().materialize(*source_realm);
    if (copied.is_error())
        return error();
    auto copied_root = GC::Root<JS::Value>::create(copied.value(), SourceLocation::current());
    GC::RootVector<JS::Value> source_arguments;
    for (size_t i = 0; i < arguments.size(); ++i)
        source_arguments.append(copied.value().as_object().get_without_side_effects(i));
    auto result = source->call(*function, source_arguments.span());
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

    auto output = BridgeData::capture(result_value, *source_realm, {});
    if (output.is_error())
        return error();
    auto answer = output.value().materialize(*target);
    if (answer.is_error())
        return error();
    return answer.release_value();
}
}
