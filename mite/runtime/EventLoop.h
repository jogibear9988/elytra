#pragma once

#include <LibGC/Function.h>
#include <LibGC/RootVector.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Runtime/VM.h>
#include <uv.h>
#include <unordered_map>

namespace Mite {

class EventLoop {
public:
    EventLoop(JS::VM&, void (*report_exception)(JS::Value));
    ~EventLoop();
    EventLoop(EventLoop const&) = delete;
    EventLoop& operator=(EventLoop const&) = delete;

    JS::ThrowCompletionOr<JS::Value> install(JS::Realm&);
    int run_until_idle();

private:
    struct Timer;
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
    GC::RootVector<GC::Ref<GC::Function<JS::ThrowCompletionOr<JS::Value>()>>> m_jobs;
    GC::RootVector<GC::Ref<JS::Promise>> m_rejections;
    std::unordered_map<uint64_t, Timer*> m_timers;
};

}
