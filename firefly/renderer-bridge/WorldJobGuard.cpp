#include "WorldJobGuard.h"
#include <LibGC/WeakHashMap.h>
#include <AK/WeakPtr.h>
#include <LibGC/WeakInlines.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/JobCallback.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Scripting/ExceptionReporter.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <AK/ScopeGuard.h>

namespace Firefly {
struct WorldJobGuard::State : public RefCounted<State> {
    JS::VM& vm;
    Function<JS::ThrowCompletionOr<JS::Value>(JS::JobCallback&, JS::Value, ReadonlySpan<JS::Value>)> upstream;
    Function<void(JS::PromiseJob, GC::Ptr<JS::Realm>)> upstream_enqueue;
    GC::WeakHashMap<JS::Realm, WeakPtr<IsolatedWorld>> realms;
    size_t blocked { 0 };
    explicit State(JS::VM& vm)
        : vm(vm), upstream(move(vm.host_call_job_callback)), upstream_enqueue(move(vm.host_enqueue_promise_job))
    {
        VERIFY(upstream); VERIFY(upstream_enqueue);
    }
    RefPtr<IsolatedWorld> owner_for(JS::Realm& realm)
    {
        auto owner = realms.get(realm);
        return owner.has_value() ? owner->strong_ref() : nullptr;
    }
    JS::ThrowCompletionOr<JS::Value> run_owned(RefPtr<IsolatedWorld> first, RefPtr<IsolatedWorld> second,
        Function<JS::ThrowCompletionOr<JS::Value>()> operation)
    {
        auto inner = [&]() -> JS::ThrowCompletionOr<JS::Value> {
            if (second && second != first && !second->is_executing()) return second->run_async_frame([&] { return operation(); });
            return operation();
        };
        if (first && !first->is_executing()) return first->run_async_frame([&] { return inner(); });
        return inner();
    }
    bool blocked_realm(JS::Realm& realm)
    {
        auto owner = owner_for(realm);
        auto& world = Web::Bindings::host_defined_wrapper_world(realm);
        auto document = Web::Bindings::principal_host_defined_environment_settings_object(realm).responsible_document();
        if (!owner || owner->is_detached() || world.is_detached() || !document || !document->is_fully_active()) {
            if (owner) owner->detach();
            else world.detach();
            ++blocked;
            return true;
        }
        return false;
    }
    void enqueue(JS::PromiseJob job, GC::Ptr<JS::Realm> realm)
    {
        GC::Ptr<JS::Realm> origin;
        vm.for_each_execution_context_top_to_bottom([&](JS::ExecutionContext const& context) {
            if (context.realm && realms.contains(*context.realm)) {
                origin = context.realm;
                return false;
            }
            return true;
        });
        bool owned_realm = realm && realms.contains(*realm);
        if (!owned_realm && !origin) {
            upstream_enqueue(move(job), realm);
            return;
        }
        if (origin && blocked_realm(*origin)) return;
        if (owned_realm && realm != origin && blocked_realm(*realm)) return;
        NonnullRefPtr self { *this };
        auto script_or_module = vm.get_active_script_or_module();
        // PromiseJob cannot be wrapped through its private constructor. Queue
        // only our owned-world jobs via the public HTML microtask/context APIs;
        // the upstream hook retains all other jobs and the VM's queue contract.
        Web::HTML::queue_a_microtask(nullptr, GC::create_function(vm.heap(),
            [self, realm, origin, owned_realm, job = move(job), script_or_module] {
                if (origin && self->blocked_realm(*origin)) return;
                if (owned_realm && realm != origin && self->blocked_realm(*realm)) return;
                // Acquire ownership before context setup can allocate. Never run
                // an unprotected job if an owner vanished after the first check.
                auto origin_owner = origin ? self->owner_for(*origin) : nullptr;
                auto realm_owner = owned_realm ? self->owner_for(*realm) : nullptr;
                if (origin && !origin_owner) { self->blocked_realm(*origin); return; }
                if (owned_realm && !realm_owner) { self->blocked_realm(*realm); return; }
                auto execution_realm = realm ? realm : origin;
                Web::HTML::TemporaryExecutionContext context(*execution_realm, Web::HTML::TemporaryExecutionContext::CallbacksEnabled::Yes);
                auto& execution = Web::Bindings::principal_host_defined_environment_settings_object(*execution_realm).realm_execution_context();
                auto previous = execution.script_or_module;
                execution.script_or_module = script_or_module;
                ScopeGuard restore([&] { execution.script_or_module = previous; });
                auto result = self->run_owned(move(origin_owner), move(realm_owner), [&] { return job.run(); });
                if (result.is_error()) Web::HTML::report_exception(result, *execution_realm);
            }));
    }

    JS::ThrowCompletionOr<JS::Value> invoke(JS::JobCallback& callback, JS::Value receiver, ReadonlySpan<JS::Value> arguments)
    {
        // This engine operation unwraps bound functions/proxies without property
        // access or traps. Revoked proxies already cannot execute: delegate their
        // normal error path to the original hook.
        auto realm = JS::get_function_realm(vm, callback.callback());
        if (!realm.is_error() && realms.contains(*realm.value())) {
            if (blocked_realm(*realm.value())) return JS::js_undefined();
            return run_owned(owner_for(*realm.value()), nullptr, [&] { return upstream(callback, receiver, arguments); });
        }
        return upstream(callback, receiver, arguments);
    }
};
WorldJobGuard::WorldJobGuard(JS::VM& vm)
    : m_state(adopt_ref(*new State(vm)))
{
    // Keep the state alive in the VM, not through the renderer session's address.
    // Session shutdown must not reopen queued callbacks by uninstalling the hook.
    vm.host_call_job_callback = [state = m_state](JS::JobCallback& callback, JS::Value receiver, ReadonlySpan<JS::Value> arguments) {
        return state->invoke(callback, receiver, arguments);
    };
    vm.host_enqueue_promise_job = [state = m_state](JS::PromiseJob job, GC::Ptr<JS::Realm> realm) {
        state->enqueue(move(job), realm);
    };
}
WorldJobGuard::~WorldJobGuard() = default;
ErrorOr<void> WorldJobGuard::protect(IsolatedWorld& owner)
{
    auto* realm = owner.realm();
    if (!realm || &realm->vm() != &m_state->vm || Web::Bindings::host_defined_wrapper_world(*realm).is_main_world())
        return Error::from_string_literal("Job guard requires a live isolated world in its VM");
    m_state->realms.set(*realm, owner.make_weak_ptr());
    return {};
}
size_t WorldJobGuard::blocked_count() const { return m_state->blocked; }
}
