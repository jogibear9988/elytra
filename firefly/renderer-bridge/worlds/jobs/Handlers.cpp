/** Own private event-handler maps and root their cells during cleanup/reentry. */
#include "worlds/jobs/State.h"
#include <LibGC/Function.h>
#include <LibGC/WeakInlines.h>
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/HTML/EventHandler.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>

namespace Firefly {
Callbacks::HandlerMap* WorldJobGuard::State::select_handler_map(Web::DOM::EventTarget& target)
{
    auto* realm = vm.current_realm();
    if (!realm || !realms.contains(*realm)) {
        if (previous_gate.select_handler_map)
            return previous_gate.select_handler_map(previous_gate.context, target);
        return nullptr;
    }

    handler_maps.remove_all_matching(
        [](auto const& record) { return !record.realm.ptr() || !record.target.ptr(); });
    for (auto& record : handler_maps)
        if (record.realm.ptr().ptr() == realm && record.target.ptr().ptr() == &target)
            return record.map.ptr();
    auto map = make<Callbacks::HandlerMap>();
    auto* result = map.ptr();
    handler_maps.append({ *realm, target, move(map) });
    auto owner = owner_for(*realm);
    if (!owner || (owner->is_detached() && !owner->is_executing())) {
        // A foreign direct call into an already revoked realm must never
        // fall back to the page map. Clear any late writes after return.
        NonnullRefPtr self { *this };
        Web::HTML::queue_a_microtask(nullptr,
            GC::create_function(vm.heap(),
                GC::captures(self, GC::Ref { *realm }, GC::Ref { target }),
                [](auto& self, auto& realm_root, auto& target_root) {
                    self->clear_handler_maps(realm_root.ptr(), target_root.ptr());
                }));
    }

    return result;
}

void WorldJobGuard::State::visit_handler_maps(
    Web::DOM::EventTarget& target, GC::Cell::Visitor& visitor)
{
    for (auto const& record : handler_maps)
        if (record.target.ptr().ptr() == &target && record.realm.ptr())
            visitor.visit(*record.map);
    if (previous_gate.visit_handler_maps)
        previous_gate.visit_handler_maps(previous_gate.context, target, visitor);
}

void WorldJobGuard::State::clear_handler_maps(JS::Realm* realm, Web::DOM::EventTarget* target)
{
    struct Removal {
        GC::Root<Web::DOM::EventTarget> target;
        OwnPtr<Callbacks::HandlerMap> map;
    };

    Vector<Removal> removed;
    handler_maps.remove_all_matching([&](auto& record) {
        if ((!realm || record.realm.ptr().ptr() == realm)
            && (!target || record.target.ptr().ptr() == target) && record.target.ptr()) {
            removed.append({ GC::Root { *record.target.ptr() }, move(record.map) });
            return true;
        }

        return !record.realm.ptr() || !record.target.ptr();
    });
    // Native EventTargets normally trace these maps. Once removed from the
    // registry, keep all handler cells rooted through native cleanup/reentry.
    Vector<GC::Root<Web::HTML::EventHandler>> roots;
    for (auto const& record : removed)
        for (auto const& entry : *record.map)
            roots.append(GC::Root { entry.value });
    for (auto const& record : removed) {
        Callbacks::clear_handler_map(*record.target, *record.map);
        ++canceled_handler_maps;
    }
}

}
