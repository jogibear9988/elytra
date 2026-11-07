/** VM hook registration handle and diagnostics; shared private policy lives in worlds/jobs/. */
#include "worlds/jobs/State.h"
#include <LibGC/WeakInlines.h>
#include <LibJS/Runtime/JobCallback.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/DOM/DOMEventListener.h>
#include <LibWeb/DOM/MutationObserver.h>
#include <LibWeb/HTML/Timer.h>

namespace Firefly {
WorldJobGuard::WorldJobGuard(JS::VM& vm)
    : m_state(adopt_ref(*new State(vm)))
{
    // Keep the state alive in the VM, not through the renderer session's address.
    // Session shutdown must not reopen queued callbacks by uninstalling the hook.
    vm.host_call_job_callback = [state = m_state](JS::JobCallback& callback, JS::Value receiver,
                                    ReadonlySpan<JS::Value> arguments) {
        return state->invoke(callback, receiver, arguments);
    };

    vm.host_enqueue_promise_job = [state = m_state](JS::PromiseJob job, GC::Ptr<JS::Realm> realm) {
        state->enqueue(move(job), realm);
    };

    vm.host_promise_rejection_tracker
        = [state = m_state](JS::Promise& promise, JS::Promise::RejectionOperation operation) {
              state->rejection(promise, operation);
          };
}

WorldJobGuard::~WorldJobGuard() = default;

ErrorOr<void> WorldJobGuard::protect(IsolatedWorld& owner)
{
    auto* realm = owner.realm();
    if (!realm || &realm->vm() != &m_state->vm
        || Web::Bindings::host_defined_wrapper_world(*realm).is_main_world())
        return Error::from_string_literal("Job guard requires a live isolated world in its VM");
    if (m_state->realms.contains(*realm))
        return {};
    m_state->realms.set(*realm, owner.make_weak_ptr());
    owner.add_detach_hook([state = m_state, realm = GC::Weak<JS::Realm> { *realm }] {
        if (realm.ptr()) {
            state->cancel_timers(*realm.ptr());
            state->cancel_observers(*realm.ptr());
            state->cancel_listeners(*realm.ptr());
            state->clear_handler_maps(realm.ptr().ptr(), nullptr);
            EventValues::revoke(*realm.ptr());
        }
    });
    return {};
}

Vector<GC::Root<Web::HTML::Timer>> WorldJobGuard::owned_timer_snapshot() const
{
    Vector<GC::Root<Web::HTML::Timer>> result;
    for (auto const& record : m_state->timers) {
        if (record.timer.ptr())
            result.append(GC::Root { *record.timer.ptr() });
    }

    return result;
}

Vector<GC::Root<Web::DOM::MutationObserver>> WorldJobGuard::owned_observer_snapshot() const
{
    Vector<GC::Root<Web::DOM::MutationObserver>> result;
    for (auto const& record : m_state->observers)
        if (record.observer.ptr())
            result.append(GC::Root { *record.observer.ptr() });
    return result;
}

Vector<GC::Root<Web::DOM::DOMEventListener>> WorldJobGuard::owned_listener_snapshot() const
{
    Vector<GC::Root<Web::DOM::DOMEventListener>> result;
    for (auto const& record : m_state->listeners)
        if (record.listener.ptr())
            result.append(GC::Root { *record.listener.ptr() });
    return result;
}

size_t WorldJobGuard::owned_handler_map_count() const
{
    return m_state->handler_maps.size();
}

size_t WorldJobGuard::canceled_handler_map_count() const
{
    return m_state->canceled_handler_maps;
}

size_t WorldJobGuard::owned_listener_count() const
{
    return m_state->listeners.size();
}

size_t WorldJobGuard::owned_listener_signal_count() const
{
    return m_state->listener_signals.size();
}

size_t WorldJobGuard::canceled_listener_count() const
{
    return m_state->canceled_listeners;
}

size_t WorldJobGuard::canceled_listener_signal_count() const
{
    return m_state->canceled_listener_signals;
}

size_t WorldJobGuard::owned_observer_count() const
{
    return m_state->observers.size();
}

size_t WorldJobGuard::canceled_observer_count() const
{
    return m_state->canceled_observers;
}

size_t WorldJobGuard::owned_timer_count() const
{
    return m_state->timers.size();
}

size_t WorldJobGuard::canceled_timer_count() const
{
    return m_state->canceled_timers;
}

size_t WorldJobGuard::blocked_count() const
{
    return m_state->blocked;
}

size_t WorldJobGuard::source_rejection_count() const
{
    return m_state->source_rejections;
}

size_t WorldJobGuard::source_job_error_count() const
{
    return m_state->source_job_errors;
}
}
