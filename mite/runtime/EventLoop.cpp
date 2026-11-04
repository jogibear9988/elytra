#include "EventLoop.h"
#include "MiteBootstrap.h"

#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibJS/Script.h>
#include <cmath>
#include <cstring>

namespace Mite {

struct EventLoop::Timer {
    Timer(EventLoop& owner, uint64_t id, JS::FunctionObject& callback, JS::Object& receiver, bool repeat)
        : owner(owner), id(id), repeating(repeat), callback(callback), receiver(receiver)
    {
    }
    EventLoop& owner;
    uint64_t id;
    bool repeating;
    uv_timer_t handle {};
    GC::Root<JS::FunctionObject> callback;
    GC::Root<JS::Object> receiver;
    GC::RootVector<JS::Value> arguments;
};

EventLoop::EventLoop(JS::VM& vm, void (*report_exception)(JS::Value))
    : m_vm(vm), m_report_exception(report_exception)
{
    VERIFY(uv_loop_init(&m_loop) == 0);
    m_vm.host_enqueue_promise_job = [this](JS::PromiseJob job, GC::Ptr<JS::Realm> realm) {
        m_jobs.append(GC::create_function(m_vm.heap(), [job, realm] {
            (void)realm;
            return job.run();
        }));
    };
    m_vm.host_promise_job_queue_is_empty = [this] { return m_jobs.is_empty(); };
    m_vm.host_promise_rejection_tracker = [this](JS::Promise& promise, JS::Promise::RejectionOperation operation) {
        if (operation == JS::Promise::RejectionOperation::Reject)
            m_rejections.append(promise);
        else
            m_rejections.remove_all_matching([&](auto const& entry) { return entry.ptr() == &promise; });
    };
}

EventLoop::~EventLoop()
{
    m_shutting_down = true;
    // Close even unreferenced timers, and release their JS roots before the VM.
    while (!m_timers.empty())
        cancel_timer(m_timers.begin()->first);
    for (auto& hook : m_shutdown_hooks) hook();
    do {
        uv_run(&m_loop, UV_RUN_DEFAULT);
    } while (uv_loop_alive(&m_loop));
    VERIFY(uv_loop_close(&m_loop) == 0);
}

struct EventLoop::Work {
    EventLoop& owner;
    std::function<void()> execute;
    GC::Root<GC::Function<JS::ThrowCompletionOr<JS::Value>()>> complete;
    uv_work_t request {};
};

JS::ThrowCompletionOr<void> EventLoop::queue_work(std::function<void()> execute,
    GC::Ref<GC::Function<JS::ThrowCompletionOr<JS::Value>()>> complete)
{
    auto* work = new Work { *this, std::move(execute), GC::Root { complete }, {} };
    work->request.data = work;
    int result = uv_queue_work(&m_loop, &work->request, [](uv_work_t* request) {
        static_cast<Work*>(request->data)->execute();
    }, [](uv_work_t* request, int status) {
        auto* work = static_cast<Work*>(request->data);
        auto& owner = work->owner;
        // Drain native requests on shutdown, but never enter application JS then.
        if (!owner.m_shutting_down && !owner.m_status) {
            if (status) {
                owner.fail(JS::Error::create(owner.m_vm.realm(), "Native work request failed"_utf16));
            } else {
                auto result = work->complete->function()();
                if (result.is_error()) owner.fail(result.error_value());
                owner.checkpoint();
            }
        }
        --owner.m_pending_work;
        delete work;
    });
    if (result) {
        delete work;
        return m_vm.throw_completion<JS::Error>(Utf16String::from_utf8(StringView(uv_strerror(result), strlen(uv_strerror(result)))));
    }
    ++m_pending_work;
    return {};
}

static JS::Completion invalid_callback(JS::VM& vm)
{
    auto error = JS::TypeError::create(vm.realm(), "The callback argument must be of type function"_utf16);
    error->define_direct_property("code"_utf16,
        JS::PrimitiveString::create(vm, "ERR_INVALID_ARG_TYPE"_utf16), JS::default_attributes);
    return JS::throw_completion(error);
}

JS::ThrowCompletionOr<JS::Value> EventLoop::install(JS::Realm& realm)
{
    auto& global = realm.global_object();
    global.define_native_function(realm, "__miteEnqueueTick"_utf16, [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto callback = vm.argument(0);
        if (!callback.is_function()) return invalid_callback(vm);
        m_ticks.append(GC::create_function(vm.heap(), [this, callback] {
            return JS::call(m_vm, callback, JS::js_undefined(), ReadonlySpan<JS::Value> {});
        }));
        return JS::js_undefined();
    }, 1, JS::default_attributes);
    global.define_native_function(realm, "queueMicrotask"_utf16, [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto callback = vm.argument(0);
        if (!callback.is_function())
            return invalid_callback(vm);
        m_jobs.append(GC::create_function(vm.heap(), [this, callback] {
            return JS::call(m_vm, callback, JS::js_undefined(), ReadonlySpan<JS::Value> {});
        }));
        return JS::js_undefined();
    }, 1, JS::default_attributes);
    global.define_native_function(realm, "__miteScheduleTimer"_utf16, [this](JS::VM&) {
        return schedule_timer();
    }, 4, JS::default_attributes);
    global.define_native_function(realm, "__miteCancelTimer"_utf16, [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto id = vm.argument(0);
        if (id.is_number() && id.as_double() >= 1 && id.as_double() < 9007199254740992.0
            && std::trunc(id.as_double()) == id.as_double())
            cancel_timer(static_cast<uint64_t>(id.as_double()));
        return JS::js_undefined();
    }, 1, JS::default_attributes);
    global.define_native_function(realm, "__miteRefTimer"_utf16, [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto id = vm.argument(0);
        if (id.is_number() && id.as_double() >= 1 && id.as_double() < 9007199254740992.0
            && std::trunc(id.as_double()) == id.as_double()) {
            auto found = m_timers.find(static_cast<uint64_t>(id.as_double()));
            if (found != m_timers.end()) {
                auto* handle = reinterpret_cast<uv_handle_t*>(&found->second->handle);
                if (vm.argument(1).to_boolean())
                    uv_ref(handle);
                else
                    uv_unref(handle);
            }
        }
        return JS::js_undefined();
    }, 2, JS::default_attributes);
    auto parsed = JS::Script::parse(Utf16String::from_utf8(StringView(MITE_BOOTSTRAP_SOURCE, sizeof(MITE_BOOTSTRAP_SOURCE) - 1)), realm, "mite:bootstrap"sv);
    VERIFY(!parsed.is_error());
    GC::Root script { parsed.release_value() };
    return m_vm.run(*script);
}

JS::ThrowCompletionOr<JS::Value> EventLoop::schedule_timer()
{
    auto callback = m_vm.argument(0);
    auto delay = m_vm.argument(1);
    auto receiver = m_vm.argument(3);
    if (!callback.is_function())
        return invalid_callback(m_vm);
    if (!delay.is_number() || !std::isfinite(delay.as_double()) || delay.as_double() < 1
        || delay.as_double() > 2147483647 || !receiver.is_object())
        return m_vm.throw_completion<JS::TypeError>("Invalid internal timer arguments"_utf16);
    auto id = m_next_timer_id++;
    if (id >= 9007199254740992ULL)
        return m_vm.throw_completion<JS::RangeError>("Timer ID range exhausted"_utf16);
    auto repeat = m_vm.argument(2).to_boolean();
    auto* timer = new Timer(*this, id, callback.as_function(), receiver.as_object(), repeat);
    for (size_t i = 4; i < m_vm.argument_count(); ++i)
        timer->arguments.append(m_vm.argument(i));
    int error = uv_timer_init(&m_loop, &timer->handle);
    if (error) {
        delete timer;
        return m_vm.throw_completion<JS::Error>(Utf16String::from_utf8(StringView(uv_strerror(error), strlen(uv_strerror(error)))));
    }
    timer->handle.data = timer;
    m_timers.emplace(id, timer);
    uv_update_time(&m_loop);
    auto milliseconds = static_cast<uint64_t>(delay.as_double());
    error = uv_timer_start(&timer->handle, dispatch_timer, milliseconds, repeat ? milliseconds : 0);
    if (error) {
        cancel_timer(id);
        return m_vm.throw_completion<JS::Error>(Utf16String::from_utf8(StringView(uv_strerror(error), strlen(uv_strerror(error)))));
    }
    return JS::Value(static_cast<double>(id));
}

void EventLoop::cancel_timer(uint64_t id)
{
    auto found = m_timers.find(id);
    if (found == m_timers.end())
        return;
    auto* timer = found->second;
    m_timers.erase(found);
    uv_timer_stop(&timer->handle);
    uv_close(reinterpret_cast<uv_handle_t*>(&timer->handle), [](uv_handle_t* handle) {
        delete static_cast<Timer*>(handle->data);
    });
}

void EventLoop::dispatch_timer(uv_timer_t* handle)
{
    auto& timer = *static_cast<Timer*>(handle->data);
    auto& owner = timer.owner;
    if (owner.m_status)
        return;
    if (!timer.repeating)
        owner.cancel_timer(timer.id);
    auto result = JS::call(owner.m_vm, *timer.callback, JS::Value(timer.receiver.ptr()),
        ReadonlySpan<JS::Value> { timer.arguments.data(), timer.arguments.size() });
    if (result.is_error())
        owner.fail(result.error_value());
    owner.checkpoint();
}

void EventLoop::dispatch(std::function<JS::ThrowCompletionOr<JS::Value>()> const& callback)
{
    if (m_shutting_down || m_status) return;
    auto result = callback();
    if (result.is_error()) fail(result.error_value());
    checkpoint();
}

void EventLoop::fail(JS::Value error)
{
    if (m_status)
        return;
    m_report_exception(error);
    m_status = 1;
    uv_stop(&m_loop);
}

void EventLoop::checkpoint()
{
    // CJS/native callbacks drain nextTick first. ESM evaluation runs inside the
    // promise queue, so ticks scheduled there wait until that queue is empty.
    do {
        while (!m_status && !m_ticks.is_empty()) {
            GC::Root tick { m_ticks.take_first() };
            auto result = tick->function()();
            if (result.is_error()) fail(result.error_value());
        }
        while (!m_status && !m_jobs.is_empty()) {
            GC::Root job { m_jobs.take_first() };
            auto result = job->function()();
            if (result.is_error()) fail(result.error_value());
        }
    } while (!m_status && !m_ticks.is_empty());
    // LibJS also marks internal await/module promises handled directly, without
    // always sending a second host notification. Consult the authoritative state.
    m_rejections.remove_all_matching([](auto const& promise) { return promise->is_handled(); });
    if (!m_status && !m_rejections.is_empty())
        fail(m_rejections.first()->result());
}

int EventLoop::run_until_idle()
{
    checkpoint();
    if (!m_status)
        uv_run(&m_loop, UV_RUN_DEFAULT);
    return m_status;
}

}
