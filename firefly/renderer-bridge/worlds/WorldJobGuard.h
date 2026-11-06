/**
 * Associates asynchronous engine work with its source world. Revoked or removed documents
 * cannot resume source jobs into a live principal world.
 */

#pragma once
#include "worlds/IsolatedWorld.h"
#include <LibJS/Runtime/VM.h>

namespace Web::DOM {
class MutationObserver;
class DOMEventListener;
}

namespace Web::HTML {
class Timer;
}

namespace Firefly {
// Install once on the VM owner thread. The VM hook retains the guard state even
// after this registration handle is destroyed, through VM teardown. Registry
// entries are weak; owners must detach worlds before dropping their own roots.
class WorldJobGuard final {
public:
    AK_ALLOC_WITH_KMALLOC;
    explicit WorldJobGuard(JS::VM&);

    ~WorldJobGuard();

    WorldJobGuard(WorldJobGuard const&) = delete;
    WorldJobGuard& operator=(WorldJobGuard const&) = delete;
    ErrorOr<void> protect(IsolatedWorld&);

    Vector<GC::Root<Web::HTML::Timer>> owned_timer_snapshot() const;

    Vector<GC::Root<Web::DOM::MutationObserver>> owned_observer_snapshot() const;

    Vector<GC::Root<Web::DOM::DOMEventListener>> owned_listener_snapshot() const;

    size_t owned_handler_map_count() const;

    size_t canceled_handler_map_count() const;

    size_t owned_listener_count() const;

    size_t owned_listener_signal_count() const;

    size_t canceled_listener_count() const;

    size_t canceled_listener_signal_count() const;

    size_t owned_observer_count() const;

    size_t canceled_observer_count() const;

    size_t owned_timer_count() const;

    size_t canceled_timer_count() const;

    size_t blocked_count() const;

    size_t source_rejection_count() const;

    size_t source_job_error_count() const;

private:
    struct State;
    NonnullRefPtr<State> m_state;
};
}
