/**
 * Bridges libuv work to LibJS jobs and Promise checkpoints. Workers hold native snapshots
 * only; rooted completions return to the VM owner thread before touching JavaScript.
 */

#include "core/EventLoop.h"
#include "MiteBootstrap.h"
#include "core/GCCaptures.h"
#include "core/ImmediateQueue.h"

#include <AK/TemporaryChange.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/FinalizationRegistry.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibJS/Script.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Mite {

struct EventLoop::Timer {
    Timer(EventLoop& owner, uint64_t id, JS::FunctionObject& callback, JS::Object& receiver,
        bool repeat)
        : owner(owner)
        , id(id)
        , repeating(repeat)
        , callback(callback)
        , receiver(receiver)
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

void EventLoop::visit_capture_edges(GC::Cell::Visitor& visitor) const
{
    visitor.visit(m_process_error_handler.ptr());
    visitor.visit(m_entry_promise.ptr());
    for (auto const& value : m_finalizers)
        visitor.visit(value);
    for (auto const& value : m_kept_values)
        visitor.visit(value);
    for (auto const& value : m_jobs)
        visitor.visit(value);
    for (auto const& value : m_ticks)
        visitor.visit(value);
    for (auto const& value : m_rejections)
        visitor.visit(value);
    for (auto const& value : m_handled_rejections)
        visitor.visit(value);
    for (auto const& entry : m_timers) {
        visitor.visit(entry.second->callback.ptr());
        visitor.visit(entry.second->receiver.ptr());
        for (auto const& value : entry.second->arguments)
            visitor.visit(value);
    }

    if (m_immediates)
        m_immediates->visit_capture_edges(visitor);
}

EventLoop::EventLoop(JS::VM& vm, void (*report_exception)(JS::Value))
    : m_vm(vm)
    , m_report_exception(report_exception)
    , m_original_finalizer_hook(move(vm.host_enqueue_finalization_registry_cleanup_job))
{
    VERIFY(uv_loop_init(&m_loop) == 0);
    m_immediates = std::make_unique<ImmediateQueue>(*this, vm);
    VERIFY(uv_async_init(&m_loop, &m_finalizer_wakeup, [](uv_async_t* handle) {
        static_cast<EventLoop*>(handle->data)->drain_finalizers();
    }) == 0);
    m_finalizer_wakeup.data = this;
    uv_unref(reinterpret_cast<uv_handle_t*>(&m_finalizer_wakeup));
    m_vm.host_enqueue_finalization_registry_cleanup_job
        = [this](JS::FinalizationRegistry& registry) {
              if (m_shutting_down || m_status)
                  return;
              m_finalizers.append(registry);
              uv_ref(reinterpret_cast<uv_handle_t*>(&m_finalizer_wakeup));
              VERIFY(uv_async_send(&m_finalizer_wakeup) == 0);
          };

    m_vm.host_enqueue_promise_job = [this](JS::PromiseJob job, GC::Ptr<JS::Realm> realm) {
        m_jobs.append(
            GC::create_function(m_vm.heap(), GC::captures(job, realm), [](auto& job, auto& realm) {
                (void)realm;
                return job.run();
            }));
    };

    m_vm.host_promise_job_queue_is_empty = [this] { return m_jobs.is_empty(); };
    m_vm.host_promise_rejection_tracker
        = [this](JS::Promise& promise, JS::Promise::RejectionOperation operation) {
              if (m_shutting_down || m_status)
                  return;
              if (operation == JS::Promise::RejectionOperation::Reject)
                  m_rejections.append(promise);
              else
                  track_handled_rejection(promise);
          };
}

EventLoop::~EventLoop()
{
    revoke_native_captures();
    m_shutting_down = true;
    m_process_error_handler = nullptr;
    m_entry_promise = nullptr;
    m_rejections.clear();
    m_handled_rejections.clear();
    m_reported_rejections.clear();
    m_vm.host_enqueue_finalization_registry_cleanup_job = move(m_original_finalizer_hook);
    m_finalizers.clear();
    m_kept_values.clear();
    m_kept_cells.clear();
    uv_close(reinterpret_cast<uv_handle_t*>(&m_finalizer_wakeup), nullptr);
    m_immediates.reset();
    // Close even unreferenced timers, and release their JS roots before the VM.
    while (!m_timers.empty())
        cancel_timer(m_timers.begin()->first);
    for (auto& hook : m_shutdown_hooks)
        hook();
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
    int result = uv_queue_work(
        &m_loop, &work->request,
        [](uv_work_t* request) { static_cast<Work*>(request->data)->execute(); },
        [](uv_work_t* request, int status) {
            auto* work = static_cast<Work*>(request->data);
            auto& owner = work->owner;
            // Drain native requests on shutdown, but never enter application JS then.
            if (!owner.m_shutting_down && !owner.m_status) {
                TemporaryChange turn(owner.m_running_turn, true);
                if (status) {
                    owner.fail(
                        JS::Error::create(owner.m_vm.realm(), "Native work request failed"_utf16));
                } else {
                    auto result = work->complete->function()();
                    if (result.is_error())
                        owner.fail(result.error_value());
                    owner.checkpoint();
                }
            }

            --owner.m_pending_work;
            delete work;
        });
    if (result) {
        delete work;
        return m_vm.throw_completion<JS::Error>(
            Utf16String::from_utf8(StringView(uv_strerror(result), strlen(uv_strerror(result)))));
    }

    ++m_pending_work;
    return {};
}

static JS::Completion invalid_callback(JS::VM& vm)
{
    auto error
        = JS::TypeError::create(vm.realm(), "The callback argument must be of type function"_utf16);
    error->define_direct_property("code"_utf16,
        JS::PrimitiveString::create(vm, "ERR_INVALID_ARG_TYPE"_utf16), JS::default_attributes);
    return JS::throw_completion(error);
}

JS::ThrowCompletionOr<JS::Value> EventLoop::install(JS::Realm& realm)
{
    auto& global = realm.global_object();
    global.define_native_function(realm, "__miteEnqueueTick"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(native_reference(*this)),
            [](auto& captured_self, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = &captured_self.get();

                auto callback = vm.argument(0);
                if (!callback.is_function())
                    return invalid_callback(vm);
                self->m_ticks.append(
                    GC::create_function(vm.heap(), GC::captures(native_reference(*self), callback),
                        [](auto& captured_self, auto& callback) {
                            auto* self = &captured_self.get();

                            return JS::call(self->m_vm, callback, JS::js_undefined(),
                                ReadonlySpan<JS::Value> {});
                        }));
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    global.define_native_function(realm, "queueMicrotask"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(native_reference(*this)),
            [](auto& captured_self, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = &captured_self.get();

                auto callback = vm.argument(0);
                if (!callback.is_function())
                    return invalid_callback(vm);
                self->m_jobs.append(
                    GC::create_function(vm.heap(), GC::captures(native_reference(*self), callback),
                        [](auto& captured_self, auto& callback) {
                            auto* self = &captured_self.get();

                            return JS::call(self->m_vm, callback, JS::js_undefined(),
                                ReadonlySpan<JS::Value> {});
                        }));
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    global.define_native_function(realm, "__miteScheduleTimer"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(native_reference(*this)),
            [](auto& captured_self, JS::VM&) {
                auto* self = &captured_self.get();
                return self->schedule_timer();
            }),
        4, JS::default_attributes);
    global.define_native_function(realm, "__miteCancelTimer"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(native_reference(*this)),
            [](auto& captured_self, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = &captured_self.get();

                auto id = vm.argument(0);
                if (id.is_number() && id.as_double() >= 1 && id.as_double() < 9007199254740992.0
                    && std::trunc(id.as_double()) == id.as_double())
                    self->cancel_timer(static_cast<uint64_t>(id.as_double()));
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    global.define_native_function(realm, "__miteRefTimer"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(native_reference(*this)),
            [](auto& captured_self, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = &captured_self.get();

                auto id = vm.argument(0);
                if (id.is_number() && id.as_double() >= 1 && id.as_double() < 9007199254740992.0
                    && std::trunc(id.as_double()) == id.as_double()) {
                    auto found = self->m_timers.find(static_cast<uint64_t>(id.as_double()));
                    if (found != self->m_timers.end()) {
                        auto* handle = reinterpret_cast<uv_handle_t*>(&found->second->handle);
                        if (vm.argument(1).to_boolean())
                            uv_ref(handle);
                        else
                            uv_unref(handle);
                    }
                }

                return JS::js_undefined();
            }),
        2, JS::default_attributes);
    m_immediates->install(realm);
    auto parsed = JS::Script::parse(Utf16String::from_utf8(StringView(
                                        MITE_BOOTSTRAP_SOURCE, sizeof(MITE_BOOTSTRAP_SOURCE) - 1)),
        realm, "mite:bootstrap"sv);
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
        return m_vm.throw_completion<JS::Error>(
            Utf16String::from_utf8(StringView(uv_strerror(error), strlen(uv_strerror(error)))));
    }

    timer->handle.data = timer;
    m_timers.emplace(id, timer);
    uv_update_time(&m_loop);
    auto milliseconds = static_cast<uint64_t>(delay.as_double());
    error = uv_timer_start(&timer->handle, dispatch_timer, milliseconds, repeat ? milliseconds : 0);
    if (error) {
        cancel_timer(id);
        return m_vm.throw_completion<JS::Error>(
            Utf16String::from_utf8(StringView(uv_strerror(error), strlen(uv_strerror(error)))));
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
    uv_close(reinterpret_cast<uv_handle_t*>(&timer->handle),
        [](uv_handle_t* handle) { delete static_cast<Timer*>(handle->data); });
}

void EventLoop::dispatch_timer(uv_timer_t* handle)
{
    auto& timer = *static_cast<Timer*>(handle->data);
    auto& owner = timer.owner;
    if (owner.m_status)
        return;
    TemporaryChange turn(owner.m_running_turn, true);
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
    if (m_shutting_down || m_status)
        return;
    TemporaryChange turn(m_running_turn, true);
    auto result = callback();
    if (result.is_error())
        fail(result.error_value());
    checkpoint();
}

void EventLoop::keep_weak_target(JS::Value value)
{
    VERIFY(value.is_object() || value.is_symbol());
    if (m_kept_cells.insert(&value.as_cell()).second)
        m_kept_values.append(value);
}

void EventLoop::drain_finalizers()
{
    // GC only enqueues registries. Invoke their JS cleanup on a later owner-thread
    // turn, keeping every registry rooted through its individual checkpoint.
    auto batch = move(m_finalizers);
    m_finalizers = {};
    uv_unref(reinterpret_cast<uv_handle_t*>(&m_finalizer_wakeup));
    if (m_shutting_down || m_status)
        return;
    for (auto registry : batch) {
        if (m_status)
            break;
        dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
            TRY(registry->cleanup());
            return JS::js_undefined();
        });
    }
}

void EventLoop::set_process_error_handler(JS::FunctionObject* callback)
{
    if (!m_shutting_down)
        m_process_error_handler = callback;
}

JS::ThrowCompletionOr<bool> EventLoop::emit_process_error(
    Utf16View kind, JS::Value value, JS::Value auxiliary)
{
    TemporaryChange turn(m_running_turn, true);
    GC::Root callback { m_process_error_handler };
    if (!callback)
        return false;
    auto value_root = GC::make_root(value);
    auto auxiliary_root = GC::make_root(auxiliary);
    auto result = TRY(JS::call(m_vm, *callback, JS::js_undefined(),
        JS::PrimitiveString::create(m_vm, kind), value_root.value(), auxiliary_root.value()));
    return result.is_boolean() && result.as_bool();
}

void EventLoop::terminate(JS::Value error, int status)
{
    if (m_status)
        return;
    auto rooted = GC::make_root(error);
    // Reporters can inspect values; publish terminal state before any such entry.
    m_status = status;
    uv_stop(&m_loop);
    m_report_exception(rooted.value());
}

void EventLoop::uncaught_exception(JS::Value error, bool from_promise)
{
    if (m_status || m_shutting_down)
        return;
    auto rooted = GC::make_root(error);
    if (m_delivering_fatal) {
        terminate(rooted.value(), 7);
        return;
    }

    TemporaryChange turn(m_running_turn, true);
    TemporaryChange delivering(m_delivering_fatal, true);
    auto origin = JS::PrimitiveString::create(
        m_vm, from_promise ? "unhandledRejection"_utf16 : "uncaughtException"_utf16);
    auto result = emit_process_error("uncaughtException"_utf16, rooted.value(), origin);
    if (result.is_error()) {
        // A monitor/exception listener throwing is terminal, never recursively
        // delivered to the same handler. Match Node's fatal-handler exit status.
        terminate(result.error_value(), 7);
    } else if (!result.value()) {
        terminate(rooted.value(), 1);
    }
}

void EventLoop::fail(JS::Value error)
{
    uncaught_exception(error);
}

bool EventLoop::was_rejection_reported(JS::Promise const& promise) const
{
    return std::any_of(m_reported_rejections.begin(), m_reported_rejections.end(),
        [&](auto const& entry) { return entry.promise.ptr().ptr() == &promise; });
}

void EventLoop::observe_entry_promise(JS::Promise& promise)
{
    m_entry_promise = &promise;
    m_entry_exception_delivered = false;
}

void EventLoop::track_handled_rejection(JS::Promise& promise)
{
    m_rejections.remove_all_matching([&](auto const& entry) { return entry.ptr() == &promise; });
    for (auto& entry : m_reported_rejections) {
        if (entry.promise.ptr().ptr() == &promise && !entry.handled) {
            entry.handled = true;
            m_handled_rejections.append(promise);
            break;
        }
    }
}

void EventLoop::process_rejections()
{
    TemporaryChange turn(m_running_turn, true);
    // Engine-internal await/module consumers can mark a promise handled without
    // issuing a second host callback. Read the authoritative promise state too.
    std::erase_if(m_reported_rejections, [](auto const& entry) { return !entry.promise.ptr(); });
    for (auto& entry : m_reported_rejections) {
        auto promise = entry.promise.ptr();
        if (!entry.handled && promise->is_handled()) {
            entry.handled = true;
            m_handled_rejections.append(*promise);
        }
    }

    auto handled = move(m_handled_rejections);
    m_handled_rejections = {};
    for (auto promise : handled) {
        if (m_status)
            break;
        // Once handled is reported this settled promise can never become
        // unhandled again. Do not retain metadata for app-retained promises.
        std::erase_if(m_reported_rejections,
            [&](auto const& entry) { return entry.promise.ptr().ptr() == promise.ptr(); });
        auto result = emit_process_error("rejectionHandled"_utf16, promise, JS::js_undefined());
        if (result.is_error())
            fail(result.error_value());
    }

    if (!m_status && m_entry_promise && !m_entry_exception_delivered
        && m_entry_promise->state() == JS::Promise::State::Rejected) {
        GC::Root entry { m_entry_promise };
        auto reason = GC::make_root(entry->result());
        m_entry_exception_delivered = true;
        m_rejections.remove_all_matching(
            [&](auto const& promise) { return promise.ptr() == entry.ptr(); });
        // Node's ESM loader promotes its entry failure directly, even when an
        // ordinary unhandledRejection listener exists. It preserves the reason.
        uncaught_exception(reason.value(), true);
    }
    // Detach the rooted batch before callbacks can synchronously attach catches,
    // reject more promises or collect garbage. Emitted promises then remain weak
    // so an unhandledRejection listener cannot accidentally pin every rejection.
    auto rejected = move(m_rejections);
    m_rejections = {};
    for (auto promise : rejected) {
        if (m_status)
            break;
        if (promise.ptr() == m_entry_promise.ptr() || promise->is_handled()
            || was_rejection_reported(*promise))
            continue;
        m_reported_rejections.push_back({ GC::Weak { *promise }, false });
        auto reason = GC::make_root(promise->result());
        auto result = emit_process_error("unhandledRejection"_utf16, reason.value(), promise);
        if (result.is_error()) {
            // Rejection notification is not itself fatal-exception delivery:
            // its listener's error can be handled by uncaughtException normally.
            fail(result.error_value());
            continue;
        }

        if (result.value())
            continue;
        bool error_like = false;
        if (reason.value().is_object()) {
            auto& object = reason.value().as_object();
            // LibJS keeps an intrinsic Error's stack in native state rather
            // than Node's own JS stack property. Preserve genuine errors too,
            // while retaining Node's observable own-stack/Proxy behavior.
            error_like = JS::Error::is_engine_class_of(object);
            if (!error_like) {
                auto own_stack = object.has_own_property("stack"_utf16);
                if (own_stack.is_error()) {
                    fail(own_stack.error_value());
                    continue;
                }

                error_like = own_stack.value();
            }
        }

        if (!error_like) {
            auto message = Utf16String::formatted(
                "This error originated either by throwing inside of an async function without a "
                "catch block, or by rejecting a promise which was not handled with .catch(). The "
                "promise rejected with the reason \"{}\".",
                reason.value().to_utf16_string_without_side_effects());
            auto error = JS::Error::create(m_vm.realm(), message);
            error->define_direct_property("name"_utf16,
                JS::PrimitiveString::create(m_vm, "UnhandledPromiseRejection"_utf16),
                JS::default_attributes);
            error->define_direct_property("code"_utf16,
                JS::PrimitiveString::create(m_vm, "ERR_UNHANDLED_REJECTION"_utf16),
                JS::default_attributes);
            reason = GC::make_root(JS::Value(error));
        }

        uncaught_exception(reason.value(), true);
    }
}

void EventLoop::checkpoint()
{
    // CJS/native callbacks drain nextTick first. ESM evaluation runs inside the
    // promise queue, so ticks scheduled there wait until that queue is empty.
    do {
        while (!m_status && !m_ticks.is_empty()) {
            GC::Root tick { m_ticks.take_first() };
            auto result = tick->function()();
            if (result.is_error())
                fail(result.error_value());
        }

        while (!m_status && !m_jobs.is_empty()) {
            GC::Root job { m_jobs.take_first() };
            auto result = job->function()();
            if (result.is_error())
                fail(result.error_value());
        }

        // ESM evaluation runs as a promise job and can schedule nextTicks.
        // Those ticks may attach a catch in this same turn; notification must
        // wait until both queues are drained, not merely the promise queue.
        if (!m_status && m_ticks.is_empty() && m_jobs.is_empty())
            process_rejections();
    } while (!m_status
        && (!m_ticks.is_empty() || !m_jobs.is_empty() || !m_rejections.is_empty()
            || !m_handled_rejections.is_empty()));
    // Match ClearKeptObjects at the end of the complete checkpoint, not between
    // Promise/nextTick jobs. The roots also cover the pinned engine's weak traces.
    m_kept_values.clear();
    m_kept_cells.clear();
    m_vm.finish_execution_generation();
}

bool EventLoop::is_alive() const
{
    return !m_status && (!m_jobs.is_empty() || !m_ticks.is_empty() || uv_loop_alive(&m_loop));
}

int EventLoop::pump_once()
{
    if (m_running_turn)
        return m_status;
    TemporaryChange turn(m_running_turn, true);
    checkpoint();
    if (!m_status)
        uv_run(&m_loop, UV_RUN_NOWAIT);
    if (!m_status)
        checkpoint();
    return m_status;
}

int EventLoop::run_until_idle()
{
    checkpoint();
    if (!m_status)
        uv_run(&m_loop, UV_RUN_DEFAULT);
    return m_status;
}

}
