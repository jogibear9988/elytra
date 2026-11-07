/** Track native timer/observer ownership and release work on world detachment. */
#include "worlds/jobs/State.h"
#include <LibGC/WeakInlines.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/MutationObserver.h>
#include <LibWeb/HTML/Timer.h>
#include <LibWeb/HTML/Window.h>

namespace Firefly {
void WorldJobGuard::State::add_observer(Web::DOM::MutationObserver& observer)
{
    auto& callback = observer.callback();
    auto& realm = callback.callback->shape().realm();
    if (!realms.contains(realm)) {
        if (previous_gate.observer_added)
            previous_gate.observer_added(previous_gate.context, observer);
        return;
    }

    for (auto const& record : observers)
        if (record.observer.ptr().ptr() == &observer)
            return;
    observers.remove_all_matching(
        [](auto const& record) { return !record.realm.ptr() || !record.observer.ptr(); });
    auto owner = owner_for(realm);
    if (!owner || (owner->is_detached() && !owner->is_executing())) {
        Callbacks::disconnect_observer(observer);
        ++canceled_observers;
        return;
    }

    observers.append({ realm, observer });
}

void WorldJobGuard::State::remove_observer(Web::DOM::MutationObserver& observer)
{
    observers.remove_all_matching(
        [&](auto const& record) { return record.observer.ptr().ptr() == &observer; });
    if (previous_gate.observer_removed)
        previous_gate.observer_removed(previous_gate.context, observer);
}

void WorldJobGuard::State::cancel_observers(JS::Realm& realm)
{
    Vector<GC::Root<Web::DOM::MutationObserver>> removed;
    observers.remove_all_matching([&](auto const& record) {
        if (record.realm.ptr().ptr() == &realm && record.observer.ptr()) {
            removed.append(GC::Root { *record.observer.ptr() });
            return true;
        }

        return !record.realm.ptr() || !record.observer.ptr();
    });
    for (auto& observer : removed) {
        Callbacks::disconnect_observer(*observer);
        ++canceled_observers;
    }
}

void WorldJobGuard::State::cancel_timer(TimerRecord const& record)
{
    if (!record.global.ptr() || !record.timer.ptr())
        return;
    GC::Root global { *record.global.ptr() };
    GC::Root timer { *record.timer.ptr() };
    Callbacks::stop_timer(*timer);
    auto current = record.map->get(record.id);
    if (current.has_value() && current->ptr() == timer.ptr())
        global->clear_timeout(record.id);
    ++canceled_timers;
}

void WorldJobGuard::State::cancel_timers(JS::Realm& realm)
{
    Vector<TimerRecord> removed;
    timers.remove_all_matching([&](auto const& record) {
        if (record.realm.ptr().ptr() == &realm) {
            removed.append(record);
            return true;
        }

        return !record.realm.ptr() || !record.global.ptr() || !record.timer.ptr();
    });
    // Native clear_timeout reenters the map observer: remove metadata first.
    for (auto const& record : removed)
        cancel_timer(record);
}

void WorldJobGuard::State::add_timer(Web::HTML::WindowOrWorkerGlobalScopeMixin& global,
    Callbacks::TimerMap& map, int id, Web::HTML::Timer& timer)
{
    for (auto const& record : timers) {
        if (record.map == &map && record.id == id && record.timer.ptr().ptr() == &timer)
            return; // interval rearm
    }

    timers.remove_all_matching([&](auto const& record) {
        return (record.map == &map && record.id == id) || !record.global.ptr()
            || !record.timer.ptr();
    });
    GC::Ptr<JS::Realm> origin;
    vm.for_each_execution_context_top_to_bottom([&](JS::ExecutionContext const& context) {
        if (context.realm && realms.contains(*context.realm)) {
            origin = context.realm;
            return false;
        }

        return true;
    });
    auto* window = as_if<Web::HTML::Window>(global.this_impl());
    if (!origin || !window) {
        if (previous_gate.added)
            previous_gate.added(previous_gate.context, global, map, id, timer);
        return;
    }

    TimerRecord record { *origin, *window, timer, &map, id };
    auto owner = owner_for(*origin);
    if (!owner || (owner->is_detached() && !owner->is_executing())) {
        // The upstream path starts a newly inserted Timer after map.set.
        // Cancel at the next checkpoint, with roots, rather than stopping
        // before start and leaving an active orphan when it subsequently arms.
        NonnullRefPtr self { *this };
        Web::HTML::queue_a_microtask(nullptr,
            GC::create_function(vm.heap(),
                [self, record, global_root = GC::Ref { *window }, timer_root = GC::Ref { timer }] {
                    (void)global_root;
                    (void)timer_root;
                    self->cancel_timer(record);
                }));
        return;
    }

    timers.append(move(record));
}

void WorldJobGuard::State::remove_timer(
    Web::HTML::WindowOrWorkerGlobalScopeMixin& global, Callbacks::TimerMap& map, int id)
{
    timers.remove_all_matching(
        [&](auto const& record) { return record.map == &map && record.id == id; });
    if (previous_gate.removed)
        previous_gate.removed(previous_gate.context, global, map, id);
}

}
