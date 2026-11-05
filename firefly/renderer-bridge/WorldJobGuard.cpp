#include "WorldJobGuard.h"
#include "CallbackGate.h"
#include <LibWeb/WebIDL/CallbackType.h>
#include <LibGC/WeakHashMap.h>
#include <LibGC/WeakHashSet.h>
#include <AK/WeakPtr.h>
#include <LibGC/WeakInlines.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/JobCallback.h>
#include <LibJS/Runtime/Promise.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/HTML/Timer.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <AK/ScopeGuard.h>

namespace Firefly {
struct WorldJobGuard::State : public RefCounted<State> {
    JS::VM& vm;
    Function<JS::ThrowCompletionOr<JS::Value>(JS::JobCallback&, JS::Value, ReadonlySpan<JS::Value>)> upstream;
    Function<void(JS::PromiseJob, GC::Ptr<JS::Realm>)> upstream_enqueue;
    Function<void(JS::Promise&, JS::Promise::RejectionOperation)> upstream_rejections;
    GC::WeakHashMap<JS::Realm, WeakPtr<IsolatedWorld>> realms;
    GC::WeakHashSet<JS::Promise> redacted_promises;
    Callbacks::Gate previous_gate;
    struct TimerRecord {
        GC::Weak<JS::Realm> realm;
        GC::Weak<Web::HTML::Window> global;
        GC::Weak<Web::HTML::Timer> timer;
        Callbacks::TimerMap* map;
        int id;
    };
    Vector<TimerRecord> timers;
    size_t canceled_timers { 0 };
    size_t blocked { 0 };
    size_t source_rejections { 0 };
    size_t source_job_errors { 0 };
    explicit State(JS::VM& vm)
        : vm(vm), upstream(move(vm.host_call_job_callback)), upstream_enqueue(move(vm.host_enqueue_promise_job)),
          upstream_rejections(move(vm.host_promise_rejection_tracker))
    {
        VERIFY(upstream); VERIFY(upstream_enqueue); VERIFY(upstream_rejections);
        previous_gate = Callbacks::exchange_gate(vm, {this,+[](void* context, Web::WebIDL::CallbackType& callback, Optional<JS::Value> receiver, Web::WebIDL::ExceptionBehavior behavior, ReadonlySpan<JS::Value> arguments) {
            return static_cast<State*>(context)->invoke_webidl(callback, move(receiver), behavior, arguments);
        },+[](void* context, Web::HTML::WindowOrWorkerGlobalScopeMixin& global, Callbacks::TimerMap& map, int id, Web::HTML::Timer& timer) {
            static_cast<State*>(context)->add_timer(global, map, id, timer);
        },+[](void* context, Web::HTML::WindowOrWorkerGlobalScopeMixin& global, Callbacks::TimerMap& map, int id) {
            static_cast<State*>(context)->remove_timer(global, map, id);
        }});
    }
    ~State() { Callbacks::restore_gate(vm, this, previous_gate); }
    void cancel_timer(TimerRecord const& record)
    {
        if (!record.global.ptr() || !record.timer.ptr()) return;
        GC::Root global { *record.global.ptr() };
        GC::Root timer { *record.timer.ptr() };
        Callbacks::stop_timer(*timer);
        auto current = record.map->get(record.id);
        if (current.has_value() && current->ptr() == timer.ptr()) global->clear_timeout(record.id);
        ++canceled_timers;
    }
    void cancel_timers(JS::Realm& realm)
    {
        Vector<TimerRecord> removed;
        timers.remove_all_matching([&](auto const& record) {
            if (record.realm.ptr().ptr() == &realm) { removed.append(record); return true; }
            return !record.realm.ptr() || !record.global.ptr() || !record.timer.ptr();
        });
        // Native clear_timeout reenters the map observer: remove metadata first.
        for (auto const& record : removed) cancel_timer(record);
    }
    void add_timer(Web::HTML::WindowOrWorkerGlobalScopeMixin& global, Callbacks::TimerMap& map, int id, Web::HTML::Timer& timer)
    {
        for (auto const& record : timers) {
            if (record.map == &map && record.id == id && record.timer.ptr().ptr() == &timer) return; // interval rearm
        }
        timers.remove_all_matching([&](auto const& record) { return (record.map == &map && record.id == id) || !record.global.ptr() || !record.timer.ptr(); });
        GC::Ptr<JS::Realm> origin;
        vm.for_each_execution_context_top_to_bottom([&](JS::ExecutionContext const& context) {
            if (context.realm && realms.contains(*context.realm)) { origin = context.realm; return false; }
            return true;
        });
        auto* window = as_if<Web::HTML::Window>(global.this_impl());
        if (!origin || !window) {
            if (previous_gate.added) previous_gate.added(previous_gate.context, global, map, id, timer);
            return;
        }
        TimerRecord record { *origin, *window, timer, &map, id };
        auto owner = owner_for(*origin);
        if (!owner || (owner->is_detached() && !owner->is_executing())) {
            // The upstream path starts a newly inserted Timer after map.set.
            // Cancel at the next checkpoint, with roots, rather than stopping
            // before start and leaving an active orphan when it subsequently arms.
            NonnullRefPtr self { *this };
            Web::HTML::queue_a_microtask(nullptr, GC::create_function(vm.heap(),
                [self, record, global_root = GC::Ref { *window }, timer_root = GC::Ref { timer }] {
                    (void)global_root; (void)timer_root;
                    self->cancel_timer(record);
                }));
            return;
        }
        timers.append(move(record));
    }
    void remove_timer(Web::HTML::WindowOrWorkerGlobalScopeMixin& global, Callbacks::TimerMap& map, int id)
    {
        timers.remove_all_matching([&](auto const& record) { return record.map == &map && record.id == id; });
        if (previous_gate.removed) previous_gate.removed(previous_gate.context, global, map, id);
    }
    JS::Completion invoke_webidl(Web::WebIDL::CallbackType& callback, Optional<JS::Value> receiver, Web::WebIDL::ExceptionBehavior behavior, ReadonlySpan<JS::Value> arguments)
    {
        auto delegate = [&] {
            if (previous_gate.invoke) return previous_gate.invoke(previous_gate.context, callback, receiver, behavior, arguments);
            return Callbacks::invoke_original(callback, receiver, behavior, arguments);
        };
        // Promise-returning and rethrowing operations need their own conversion
        // policy. This gate protects report-style function callbacks first.
        if (behavior != Web::WebIDL::ExceptionBehavior::Report || callback.operation_returns_promise != Web::WebIDL::OperationReturnsPromise::No) return delegate();
        auto& associated = callback.callback->shape().realm();
        GC::Ptr<JS::Realm> actual;
        if (callback.callback->is_function()) {
            auto function_realm = JS::get_function_realm(vm, static_cast<JS::FunctionObject&>(*callback.callback));
            if (!function_realm.is_error()) actual = function_realm.value();
        }
        bool own_associated = realms.contains(associated);
        bool own_actual = actual && realms.contains(*actual);
        if (!own_associated && !own_actual) return delegate();
        if (own_associated && blocked_realm(associated)) return JS::js_undefined();
        if (own_actual && actual.ptr() != &associated && blocked_realm(*actual)) return JS::js_undefined();
        auto result = run_owned(own_associated ? owner_for(associated) : nullptr, own_actual ? owner_for(*actual) : nullptr,
            [&]() -> JS::ThrowCompletionOr<JS::Value> {
                // Capture the abrupt completion before the upstream Report path
                // can put its raw source object into a principal ErrorEvent.
                auto completion = Callbacks::invoke_original(callback, receiver, Web::WebIDL::ExceptionBehavior::Rethrow, arguments);
                if (completion.type() == JS::Completion::Type::Throw) return JS::throw_completion(completion.release_value());
                return completion.release_value();
            });
        if (result.is_error()) { ++source_job_errors; return JS::js_undefined(); }
        return result.release_value();
    }
    void rejection(JS::Promise& promise, JS::Promise::RejectionOperation operation)
    {
        // Native identities only: never read/stringify a source reason or place
        // its Promise in the principal Window's rejection-event lists. A foreign
        // Promise constructor can still be rejected by an owned source frame.
        bool owned = realms.contains(promise.shape().realm());
        if (operation == JS::Promise::RejectionOperation::Reject) {
            if (!owned) {
                vm.for_each_execution_context_top_to_bottom([&](JS::ExecutionContext const& context) {
                    if (context.realm && realms.contains(*context.realm)) { owned = true; return false; }
                    return true;
                });
            }
            if (owned) {
                redacted_promises.set(promise);
                ++source_rejections;
                return;
            }
        } else if (redacted_promises.remove(promise) || owned) {
            return;
        }
        upstream_rejections(promise, operation);
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
                // The ordinary reporter may dispatch an ErrorEvent with the raw
                // source object at the shared principal Window. Record only the
                // fact of failure; no source reason/stack enters the page.
                if (result.is_error()) ++self->source_job_errors;
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
    vm.host_promise_rejection_tracker = [state = m_state](JS::Promise& promise, JS::Promise::RejectionOperation operation) {
        state->rejection(promise, operation);
    };
}
WorldJobGuard::~WorldJobGuard() = default;
ErrorOr<void> WorldJobGuard::protect(IsolatedWorld& owner)
{
    auto* realm = owner.realm();
    if (!realm || &realm->vm() != &m_state->vm || Web::Bindings::host_defined_wrapper_world(*realm).is_main_world())
        return Error::from_string_literal("Job guard requires a live isolated world in its VM");
    if (m_state->realms.contains(*realm)) return {};
    m_state->realms.set(*realm, owner.make_weak_ptr());
    owner.add_detach_hook([state = m_state, realm = GC::Weak<JS::Realm> { *realm }] {
        if (realm.ptr()) state->cancel_timers(*realm.ptr());
    });
    return {};
}
Vector<GC::Root<Web::HTML::Timer>> WorldJobGuard::owned_timer_snapshot() const
{
    Vector<GC::Root<Web::HTML::Timer>> result;
    for (auto const& record : m_state->timers) {
        if (record.timer.ptr()) result.append(GC::Root { *record.timer.ptr() });
    }
    return result;
}
size_t WorldJobGuard::owned_timer_count() const { return m_state->timers.size(); }
size_t WorldJobGuard::canceled_timer_count() const { return m_state->canceled_timers; }
size_t WorldJobGuard::blocked_count() const { return m_state->blocked; }
size_t WorldJobGuard::source_rejection_count() const { return m_state->source_rejections; }
size_t WorldJobGuard::source_job_error_count() const { return m_state->source_job_errors; }
}
