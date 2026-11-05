#pragma once

#include <LibGC/Function.h>
#include <LibGC/RootVector.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Runtime/VM.h>
#include <uv.h>
#include <unordered_map>
#include <functional>
#include <vector>

namespace Mite {

class EventLoop {
public:
    EventLoop(JS::VM&, void (*report_exception)(JS::Value));
    ~EventLoop();
    EventLoop(EventLoop const&) = delete;
    EventLoop& operator=(EventLoop const&) = delete;

    JS::ThrowCompletionOr<JS::Value> install(JS::Realm&);
    int run_until_idle();
    // GUI hosts call this on the owner thread without blocking their native loop.
    int pump_once();
    bool is_alive() const;
    int status() const { return m_status; }
    // Native services share this loop and enter JS only through dispatch().
    uv_loop_t& native_loop() { return m_loop; }
    void dispatch(std::function<JS::ThrowCompletionOr<JS::Value>()> const&);
    void on_shutdown(std::function<void()> callback) { m_shutdown_hooks.push_back(std::move(callback)); }
    // Requests that may be running in the OS/thread pool must participate in
    // fatal-exit accounting even when they do not use queue_work().
    void request_started() { ++m_pending_work; }
    void request_finished() { VERIFY(m_pending_work); --m_pending_work; }
    bool has_pending_work() const { return m_pending_work != 0; }
    JS::ThrowCompletionOr<void> queue_work(std::function<void()>, GC::Ref<GC::Function<JS::ThrowCompletionOr<JS::Value>()>>);

private:
    struct Timer;
    struct Work;
    JS::ThrowCompletionOr<JS::Value> schedule_timer();
    void cancel_timer(uint64_t);
    void checkpoint();
    void fail(JS::Value);
    static void dispatch_timer(uv_timer_t*);

    JS::VM& m_vm;
    void (*m_report_exception)(JS::Value);
    uv_loop_t m_loop {};
    uint64_t m_next_timer_id { 1 };
    int m_status { 0 };
    bool m_shutting_down { false };
    size_t m_pending_work { 0 };
    std::vector<std::function<void()>> m_shutdown_hooks;
    GC::RootVector<GC::Ref<GC::Function<JS::ThrowCompletionOr<JS::Value>()>>> m_jobs;
    GC::RootVector<GC::Ref<GC::Function<JS::ThrowCompletionOr<JS::Value>()>>> m_ticks;
    GC::RootVector<GC::Ref<JS::Promise>> m_rejections;
    std::unordered_map<uint64_t, Timer*> m_timers;
};

}
