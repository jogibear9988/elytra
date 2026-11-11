/**
 * Bridges libuv work to LibJS jobs and Promise checkpoints. Workers hold native snapshots
 * only; rooted completions return to the VM owner thread before touching JavaScript.
 */

#pragma once

#include "core/GCCaptures.h"
#include "core/async/Context.h"
#include "platform/UV.h"
#include <LibGC/Function.h>
#include <LibGC/Root.h>
#include <LibGC/RootVector.h>
#include <LibGC/Weak.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Runtime/VM.h>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Mite {
class ImmediateQueue;

class EventLoop : public NativeCaptureOwner<EventLoop> {
public:
    void visit_capture_edges(GC::Cell::Visitor&) const;

    EventLoop(JS::VM&, void (*report_exception)(JS::Value));

    ~EventLoop();

    EventLoop(EventLoop const&) = delete;
    EventLoop& operator=(EventLoop const&) = delete;

    JS::ThrowCompletionOr<JS::Value> install(JS::Realm&);

    // Weak targets must remain rooted through synchronous code and its microtasks.
    void keep_weak_target(JS::Value);

    AsyncContext& async_context()
    {
        return *m_async_context;
    }

    int run_until_idle();
    // GUI hosts call this on the owner thread without blocking their native loop.
    int pump_once();

    bool is_alive() const;

    // Trusted process bootstrap only. The dispatcher returns true iff application
    // listeners handled this event; native code owns fatal/rejection semantics.
    void set_process_error_handler(JS::FunctionObject*);

    void uncaught_exception(JS::Value, bool from_promise = false);

    bool was_rejection_reported(JS::Promise const&) const;

    // Loader failure is a terminal entry exception, not a user-created promise's
    // unhandledRejection event. Register before the first checkpoint.
    void observe_entry_promise(JS::Promise&);

    void set_exception_reporter(std::function<void(JS::Value)> reporter)
    {
        m_report_exception = std::move(reporter);
    }

    int status() const
    {
        return m_status;
    }

    bool is_busy() const
    {
        return m_running_turn;
    }

    // Native services share this loop and enter JS only through dispatch().
    uv_loop_t& native_loop()
    {
        return m_loop;
    }

    void dispatch(std::function<JS::ThrowCompletionOr<JS::Value>()> const&);

    void on_shutdown(std::function<void()> callback)
    {
        m_shutdown_hooks.push_back(std::move(callback));
    }

    // Requests that may be running in the OS/thread pool must participate in
    // fatal-exit accounting even when they do not use queue_work().
    void request_started()
    {
        ++m_pending_work;
    }

    void request_finished()
    {
        VERIFY(m_pending_work);

        --m_pending_work;
    }

    bool has_pending_work() const
    {
        return m_pending_work != 0;
    }

    JS::ThrowCompletionOr<void> queue_work(
        std::function<void()>, GC::Ref<GC::Function<JS::ThrowCompletionOr<JS::Value>()>>);

private:
    void drain_finalizers();

    struct Timer;
    struct Work;
    JS::ThrowCompletionOr<JS::Value> schedule_timer();

    void cancel_timer(uint64_t);

    void checkpoint();

    void fail(JS::Value);

    void terminate(JS::Value, int status);

    JS::ThrowCompletionOr<bool> emit_process_error(Utf16View kind, JS::Value, JS::Value auxiliary);

    void process_rejections();

    void track_handled_rejection(JS::Promise&);

    static void dispatch_timer(uv_timer_t*);

    JS::VM& m_vm;
    std::unique_ptr<AsyncContext> m_async_context;
    std::function<void(JS::Value)> m_report_exception;

    uv_loop_t m_loop {};
    uv_async_t m_finalizer_wakeup {};
    Function<void(JS::FinalizationRegistry&)> m_original_finalizer_hook;
    GC::RootVector<GC::Ref<JS::FinalizationRegistry>> m_finalizers;
    GC::RootVector<JS::Value> m_kept_values;
    std::unordered_set<GC::Cell*> m_kept_cells;
    std::unique_ptr<ImmediateQueue> m_immediates;
    uint64_t m_next_timer_id { 1 };
    int m_status { 0 };
    bool m_shutting_down { false };
    bool m_running_turn { false };
    size_t m_pending_work { 0 };
    std::vector<std::function<void()>> m_shutdown_hooks;
    GC::RootVector<GC::Ref<GC::Function<JS::ThrowCompletionOr<JS::Value>()>>> m_jobs;
    GC::RootVector<GC::Ref<GC::Function<JS::ThrowCompletionOr<JS::Value>()>>> m_ticks;
    GC::RootVector<GC::Ref<JS::Promise>> m_rejections;
    GC::RootVector<GC::Ref<JS::Promise>> m_handled_rejections;

    struct ReportedRejection {
        GC::Weak<JS::Promise> promise;
        bool handled { false };
    };

    std::vector<ReportedRejection> m_reported_rejections;
    GC::Root<JS::FunctionObject> m_process_error_handler;
    GC::Root<JS::Promise> m_entry_promise;
    int m_diagnostic_entry_state { -1 };
    bool m_entry_exception_delivered { false };
    bool m_delivering_fatal { false };
    std::unordered_map<uint64_t, Timer*> m_timers;
};

}
