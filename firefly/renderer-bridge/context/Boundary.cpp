/** Shared destination-owned errors and clean realm frames for cross-world calls/settlement. */
#include "Boundary.h"
#include <AK/ScopeGuard.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/DataView.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibWeb/HTML/Scripting/Environments.h>

namespace Firefly::Boundary {
GC::Ref<JS::TypeError> error(JS::Realm& target, Utf16View message)
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

void settle(JS::PromiseCapability& capability, JS::Realm& target, JS::Value value, bool reject)
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

void reject(JS::PromiseCapability& capability, JS::Realm& target)
{
    GC::Root failure { error(target, u"Firefly bridge promise rejected"sv) };
    settle(capability, target, failure.ptr(), true);
}

// Promise resolving functions perform Get(value, "then"). Refuse a polluted
// target prototype before that publication point, so no caller getter/thenable
// can steal a still-pending settlement from the revocation registry.
bool resolve_safe(JS::Value value)
{
    if (!value.is_object())
        return true;
    auto const* object = &value.as_object();
    for (size_t depth = 0; object && depth < 64; ++depth, object = object->prototype()) {
        auto id = object->engine_class_id();
        bool binary = JS::TypedArrayBase::is_engine_class_of(*object)
            || JS::DataView::is_engine_class_of(*object) || id == JS_LAYOUT_CLASS_ID_ARRAY_BUFFER;
        // Engine prototype classes use ordinary descriptor storage, but have
        // distinct IDs. Inspect their then descriptors without executing getters.
        switch (id) {
        case JS_LAYOUT_CLASS_ID_ARRAY_BUFFER_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_DATA_VIEW_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_TYPED_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_UINT8_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_UINT8_CLAMPED_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_UINT16_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_UINT32_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_BIG_UINT64_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_INT8_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_INT16_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_INT32_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_BIG_INT64_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_FLOAT16_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_FLOAT32_ARRAY_PROTOTYPE:
        case JS_LAYOUT_CLASS_ID_FLOAT64_ARRAY_PROTOTYPE:
            binary = true;
            break;
        default:
            break;
        }

        if (JS::host_class_of(*object)
            || (!binary && id != JS_LAYOUT_CLASS_ID_OBJECT && id != JS_LAYOUT_CLASS_ID_ARRAY
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

bool caller_is(JS::VM& vm, JS::Realm& realm)
{
    bool result = false;
    vm.for_each_execution_context_top_to_bottom([&](JS::ExecutionContext const& context) {
        if (context.function && JS::NativeFunction::is_engine_class_of(*context.function))
            return true;
        result = context.realm.ptr() == &realm;
        return false;
    });
    return result;
}

JS::ThrowCompletionOr<JS::Value> in_realm(
    JS::Realm& realm, Function<JS::ThrowCompletionOr<JS::Value>()> operation)
{
    auto& vm = realm.vm();
    vm.save_execution_context_stack();
    ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
    // Promise job callbacks need an incumbent principal settings object even
    // when no script frame remains after removing the privileged source stack.
    auto context = JS::ExecutionContext::create(0, ReadonlySpan<JS::Value> {}, 0);
    context->realm = &realm;
    vm.push_execution_context(*context);
    ScopeGuard pop([&] { VERIFY(vm.pop_execution_context() == context.ptr()); });
    auto& settings = Web::HTML::principal_realm_settings_object(realm);
    Web::HTML::prepare_to_run_callback(settings);
    ScopeGuard callback([&] { Web::HTML::clean_up_after_running_callback(settings); });
    // Do not use TemporaryExecutionContext here: its script cleanup would run a
    // microtask checkpoint before the suspended outer JavaScript call returns.
    return operation();
}
}
