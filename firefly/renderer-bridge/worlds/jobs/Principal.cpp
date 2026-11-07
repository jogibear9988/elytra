/** Keep principal execution/error reporting separate from owned source frames. */
#include "worlds/jobs/State.h"
#include <AK/ScopeGuard.h>
#include <LibGC/WeakInlines.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/WebIDL/CallbackType.h>

namespace Firefly {
bool WorldJobGuard::State::has_source_context()
{
    bool found = false;
    vm.for_each_execution_context_top_to_bottom([&](JS::ExecutionContext const& context) {
        if (context.realm
            && EventValues::activity(*context.realm) != EventValues::Activity::Unregistered) {
            found = true;
            return false;
        }

        return true;
    });
    return found;
}

JS::Completion WorldJobGuard::State::in_principal(
    JS::Realm& realm, Function<JS::Completion()> operation)
{
    if (!has_source_context())
        return operation();
    // The VM keeps saved contexts rooted. Source owner frames remain active
    // in their native callers; restore them before those callers unwind.
    vm.save_execution_context_stack();
    ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
    auto context = JS::ExecutionContext::create(0, ReadonlySpan<JS::Value> {}, 0);
    context->realm = &realm;
    vm.push_execution_context(*context);
    ScopeGuard pop([&] { VERIFY(vm.pop_execution_context() == context.ptr()); });
    return operation();
}

void WorldJobGuard::State::report_exception(
    Web::WebIDL::CallbackType& callback, JS::Value exception)
{
    auto& realm = Callbacks::callback_realm(callback);
    auto activity = EventValues::activity(realm);
    GC::Ptr<JS::Realm> actual;
    if (callback.callback->is_function()) {
        auto result
            = JS::get_function_realm(vm, static_cast<JS::FunctionObject&>(*callback.callback));
        if (!result.is_error())
            actual = result.value();
    }

    if (activity != EventValues::Activity::Unregistered
        || (actual && EventValues::activity(*actual) != EventValues::Activity::Unregistered)) {
        ++source_job_errors;
        return;
    }

    auto root = GC::Root<JS::Value>::create(exception, SourceLocation::current());
    auto completion = in_principal(realm, [&]() -> JS::Completion {
        if (previous_gate.report_exception)
            previous_gate.report_exception(previous_gate.context, callback, root.value());
        else
            Callbacks::report_exception_original(callback, root.value());
        return JS::js_undefined();
    });
    VERIFY(!completion.is_error());
}

}
