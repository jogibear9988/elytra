/** Worker threads own their VM, heap and libuv loop until complete native teardown.
 * Cross-thread state contains native bytes only; no JS value or GC handle crosses.
 */
#include "core/workers/Host.h"
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include "core/Runtime.h"
#include "platform/process/ProcessHost.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Mite {
namespace {
    constexpr size_t frame_limit = 8 * 1024 * 1024;
    constexpr size_t queue_limit = 16 * 1024 * 1024;
    constexpr size_t count_limit = 64;
    constexpr unsigned worker_limit = 32;
    std::atomic<unsigned> active_workers {};
    std::atomic<uint32_t> next_thread_id { 1 };

    uint32_t allocate_thread_id()
    {
        auto next = next_thread_id.load();
        while (next <= 0x7fffffff) {
            if (next_thread_id.compare_exchange_weak(next, next + 1))
                return next;
        }

        return 0;
    }

    using Bytes = std::vector<u8>;

    struct Event {
        enum Kind { Online, Message, Error, Exit } kind;

        Bytes bytes;
        std::string name, message, stack;
        int code {};
        std::string error_code {};
    };

    // Protected VM/async pointers are borrowed only while holding mutex. The
    // owner clears them before closing its loop or destroying its VM.
    struct Channel {
        std::mutex mutex;
        std::deque<Bytes> incoming;
        std::deque<Event> outgoing;
        size_t incoming_bytes {}, outgoing_bytes {};
        JS::VM* vm {};
        uv_async_t* child_wake {};
        uv_async_t* parent_wake {};
        bool stopping {}, closed {}, port_closed {}, finished {};
        uint32_t thread_id {};
        std::string name;

        void interrupt()
        {
            std::lock_guard lock(mutex);
            stopping = true;
            if (vm)
                vm->request_interrupt();
            if (child_wake)
                uv_async_send(child_wake);
        }

        void notify(Event event)
        {
            std::lock_guard lock(mutex);
            if (closed)
                return;
            if (event.kind == Event::Message) {
                VERIFY(outgoing.size() < count_limit
                    && outgoing_bytes + event.bytes.size() <= queue_limit);
                outgoing_bytes += event.bytes.size();
            }

            outgoing.push_back(std::move(event));
            if (parent_wake)
                uv_async_send(parent_wake);
        }
    };

    struct ChildEndpoint;
    thread_local std::shared_ptr<ChildEndpoint> current_child;

    JS::Value text(JS::VM& vm, std::string const& value)
    {
        return JS::PrimitiveString::create(vm,
            Utf16String::from_utf8_with_replacement_character(
                StringView(value.data(), value.size())));
    }

    JS::ThrowCompletionOr<std::string> string(JS::VM& vm, JS::Value value, size_t limit)
    {
        if (!value.is_string())
            return vm.throw_completion<JS::TypeError>("Worker string required"_utf16);
        auto view = value.as_string().utf16_string_view();
        for (size_t i = 0; i < view.length_in_code_units(); ++i) {
            auto unit = view.code_unit_at(i);
            if (unit >= 0xd800 && unit <= 0xdbff) {
                if (++i == view.length_in_code_units() || view.code_unit_at(i) < 0xdc00
                    || view.code_unit_at(i) > 0xdfff)
                    return vm.throw_completion<JS::TypeError>("Invalid worker Unicode"_utf16);
            } else if (unit >= 0xdc00 && unit <= 0xdfff) {
                return vm.throw_completion<JS::TypeError>("Invalid worker Unicode"_utf16);
            }
        }

        auto bytes = MUST(view.to_byte_string());
        if (bytes.length() > limit || bytes.contains('\0'))
            return vm.throw_completion<JS::TypeError>("Invalid worker string"_utf16);
        return std::string(bytes.characters(), bytes.length());
    }

    JS::ThrowCompletionOr<Bytes> payload(JS::VM& vm, JS::Realm& realm, JS::Value value)
    {
        if (!value.is_object() || !JS::Uint8Array::is_engine_class_of(value.as_object()))
            return vm.throw_completion<JS::TypeError>("Worker bytes required"_utf16);
        auto& array = static_cast<JS::Uint8Array&>(value.as_object());
        if (JS::host_class_of(array) || &array.shape().realm() != &realm)
            return vm.throw_completion<JS::TypeError>("Foreign worker bytes"_utf16);
        auto* buffer = array.viewed_array_buffer();
        if (!buffer || buffer->is_detached() || buffer->is_shared_array_buffer()
            || !buffer->is_fixed_length())
            return vm.throw_completion<JS::TypeError>(
                "Worker bytes require fixed private storage"_utf16);
        auto witness = JS::make_typed_array_with_buffer_witness_record(
            array, JS::ArrayBuffer::Order::SeqCst);
        if (JS::is_typed_array_out_of_bounds(witness))
            return vm.throw_completion<JS::TypeError>("Detached worker bytes"_utf16);
        auto size = JS::typed_array_byte_length(witness);
        if (!size || size > frame_limit)
            return vm.throw_completion<JS::RangeError>("Worker message byte limit"_utf16);
        return buffer->with_readonly_bytes(array.byte_offset(), size,
            [](ReadonlyBytes bytes) { return Bytes(bytes.begin(), bytes.end()); });
    }

    JS::ThrowCompletionOr<GC::Ref<JS::Uint8Array>> materialize(JS::Realm& realm, Bytes const& bytes)
    {
        auto result = TRY(JS::Uint8Array::create(realm, bytes.size()));
        result->viewed_array_buffer()->overwrite(0, bytes.data(), bytes.size());
        return result;
    }

    struct ChildEndpoint {
        std::shared_ptr<Channel> channel;
        EventLoop* loop {};
        JS::Realm* realm {};
        GC::Root<JS::FunctionObject> callback;
        uv_async_t wake {};
        bool initialized {}, subscribed {}, stopped {};

        void visit_capture_edges(GC::Cell::Visitor& visitor) const
        {
            visitor.visit(callback.ptr());
        }

        void drain()
        {
            if (stopped || !callback)
                return;
            for (size_t i = 0; i < count_limit && callback && !stopped; ++i) {
                Bytes bytes;
                {
                    std::lock_guard lock(channel->mutex);
                    if (channel->incoming.empty() || channel->stopping)
                        break;
                    bytes = std::move(channel->incoming.front());
                    channel->incoming.pop_front();
                    channel->incoming_bytes -= bytes.size();
                }

                GC::Root handler { callback.ptr() };
                loop->dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                    GC::Root value { TRY(materialize(*realm, bytes)) };
                    return JS::call(realm->vm(), *handler, JS::js_undefined(),
                        text(realm->vm(), "message"), value.ptr());
                });
            }
        }

        void attach(JS::VM& vm, JS::Realm& owner_realm, EventLoop& owner_loop)
        {
            VERIFY(!initialized);
            realm = &owner_realm;
            loop = &owner_loop;
            VERIFY(uv_async_init(&loop->native_loop(), &wake, [](uv_async_t* handle) {
                auto& endpoint = *static_cast<ChildEndpoint*>(handle->data);
                bool stopping;
                {
                    std::lock_guard lock(endpoint.channel->mutex);
                    stopping = endpoint.channel->stopping;
                }

                if (stopping)
                    uv_stop(&endpoint.loop->native_loop());
                else
                    endpoint.drain();
            }) == 0);
            wake.data = this;
            uv_unref(reinterpret_cast<uv_handle_t*>(&wake));
            initialized = true;
            std::lock_guard lock(channel->mutex);
            channel->vm = &vm;
            channel->child_wake = &wake;
            if (channel->stopping)
                vm.request_interrupt();
        }

        void stop()
        {
            if (stopped)
                return;
            stopped = true;
            callback = nullptr;
            {
                std::lock_guard lock(channel->mutex);
                channel->child_wake = nullptr;
                channel->vm = nullptr;
                channel->incoming.clear();
                channel->incoming_bytes = 0;
            }

            if (initialized && !uv_is_closing(reinterpret_cast<uv_handle_t*>(&wake)))
                uv_close(reinterpret_cast<uv_handle_t*>(&wake), nullptr);
            realm = nullptr;
            loop = nullptr;
        }
    };

    std::string diagnostic(JS::Value value)
    {
        auto bytes = value.to_utf16_string_without_side_effects().to_byte_string();
        return std::string(bytes.characters(), std::min(bytes.length(), size_t { 65536 }));
    }

    void run_child(std::shared_ptr<Channel> channel, OwnerSession session)
    {
        // OS labels are byte-limited (15 bytes on Linux); retain the complete
        // requested name separately for the realm-local threadName property.
        auto label = channel->name.empty()
            ? std::string("mite-worker-") + std::to_string(channel->thread_id)
            : channel->name;
        size_t length = std::min(label.size(), size_t { 15 });
        while (length < label.size() && (static_cast<unsigned char>(label[length]) & 0xc0) == 0x80)
            --length;
        label.resize(length);
        auto name_status = uv_thread_setname(label.c_str());
        if (name_status) {
            Event failure { Event::Error, {}, "Error",
                std::string("Cannot name native worker: ") + uv_strerror(name_status), {}, 0 };
            failure.error_code = "ERR_WORKER_INIT_FAILED";
            channel->notify(std::move(failure));
            {
                std::lock_guard lock(channel->mutex);
                channel->finished = true;
            }

            channel->notify({ Event::Exit, {}, {}, {}, {}, 1 });
            return;
        }

        auto endpoint = std::make_shared<ChildEndpoint>();
        endpoint->channel = channel;
        current_child = endpoint;
        HostHooks hooks;
        hooks.terminate_pending_work_on_stop = true;
        hooks.initialize = [channel, endpoint](auto& vm, auto& realm, auto&,
                               auto& loop) -> JS::ThrowCompletionOr<void> {
            // Termination must work even when the entry never imports this
            // builtin (including an immediate infinite JavaScript loop).
            if (!endpoint->initialized)
                endpoint->attach(vm, realm, loop);
            channel->notify({ Event::Online, {}, {}, {}, {}, 0 });
            return {};
        };

        hooks.shutdown = [endpoint] { endpoint->stop(); };
        hooks.exception = [channel](JS::Value value) {
            {
                std::lock_guard lock(channel->mutex);
                if (channel->stopping)
                    return;
            }

            Event event { Event::Error, {}, "Error", diagnostic(value), {}, 0 };
            if (value.is_object() && JS::Error::is_engine_class_of(value.as_object())) {
                auto& error = static_cast<JS::Error&>(value.as_object());
                auto name = error.get_without_side_effects("name"_utf16);
                auto message = error.get_without_side_effects("message"_utf16);
                if (name.is_string())
                    event.name = diagnostic(name);
                if (message.is_string())
                    event.message = diagnostic(message);
                auto stack = error.stack_string().to_byte_string();
                event.stack.assign(stack.characters(), std::min(stack.length(), size_t { 65536 }));
            }

            channel->notify(std::move(event));
        };

        int code = run_owner_session(session, hooks);
        // Early bootstrap failures may occur before hooks.shutdown is installed.
        // No live VM pointer is published before the host is attached.
        VERIFY(!endpoint->initialized || endpoint->stopped);
        current_child.reset();
        {
            std::lock_guard lock(channel->mutex);
            channel->finished = true;
            if (channel->stopping)
                code = 1;
        }

        channel->notify({ Event::Exit, {}, {}, {}, {}, code });
    }

    struct Service;

    struct Worker {
        Service& owner;
        uint32_t id;
        std::shared_ptr<Channel> channel;
        GC::Root<JS::FunctionObject> callback;
        GC::Root<AsyncCarrier> context;
        uv_async_t wake {};
        uv_thread_t thread {};
        bool started {}, closing {};
    };

    struct Service {
        JS::VM& vm;
        JS::Realm& realm;
        EventLoop& loop;
        std::shared_ptr<ChildEndpoint> child;
        std::unordered_map<uint32_t, Worker*> workers;
        bool stopping {};

        void visit_capture_edges(GC::Cell::Visitor& visitor) const
        {
            if (stopping)
                return;
            visitor.visit(realm);
            if (child)
                child->visit_capture_edges(visitor);
            for (auto const& [id, worker] : workers) {
                visitor.visit(worker->callback.ptr());
                visitor.visit(worker->context.ptr());
            }
        }

        Worker* get(JS::Value value)
        {
            if (!value.is_number() || !std::isfinite(value.as_double()) || value.as_double() < 1
                || value.as_double() > 0x7fffffff
                || std::trunc(value.as_double()) != value.as_double())
                return nullptr;
            auto found = workers.find(static_cast<uint32_t>(value.as_double()));
            return found == workers.end() ? nullptr : found->second;
        }

        void close(Worker& worker)
        {
            if (worker.closing)
                return;
            worker.closing = true;
            worker.callback = nullptr;
            worker.context = nullptr;
            {
                std::lock_guard lock(worker.channel->mutex);
                worker.channel->closed = true;
                worker.channel->parent_wake = nullptr;
                worker.channel->outgoing.clear();
                worker.channel->outgoing_bytes = 0;
            }

            worker.channel->interrupt();
            if (worker.started)
                VERIFY(uv_thread_join(&worker.thread) == 0);
            uv_close(reinterpret_cast<uv_handle_t*>(&worker.wake), [](uv_handle_t* handle) {
                auto* worker = static_cast<Worker*>(handle->data);
                worker->owner.workers.erase(worker->id);
                delete worker;
                --active_workers;
            });
        }

        void drain(Worker& worker)
        {
            for (size_t i = 0; i < count_limit + 3 && !stopping && !worker.closing; ++i) {
                Event event;
                {
                    std::lock_guard lock(worker.channel->mutex);
                    if (worker.channel->outgoing.empty())
                        break;
                    event = std::move(worker.channel->outgoing.front());
                    worker.channel->outgoing.pop_front();
                    worker.channel->outgoing_bytes -= event.bytes.size();
                }

                GC::Root handler { worker.callback.ptr() };
                if (!handler)
                    break;
                loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                    AsyncContext::Scope context(loop.async_context(), worker.context.ptr());
                    JS::Value value = JS::js_undefined();
                    GC::Root<JS::Object> object;
                    char const* kind = "online";
                    if (event.kind == Event::Message) {
                        kind = "message";
                        object = TRY(materialize(realm, event.bytes)).ptr();
                        value = object.ptr();
                    } else if (event.kind == Event::Error) {
                        kind = "error";
                        object = JS::Object::create(realm, nullptr);
                        object->define_direct_property(
                            "name"_utf16, text(vm, event.name), JS::default_attributes);
                        object->define_direct_property(
                            "message"_utf16, text(vm, event.message), JS::default_attributes);
                        object->define_direct_property(
                            "stack"_utf16, text(vm, event.stack), JS::default_attributes);
                        if (!event.error_code.empty())
                            object->define_direct_property(
                                "code"_utf16, text(vm, event.error_code), JS::default_attributes);
                        value = object.ptr();
                    } else if (event.kind == Event::Exit) {
                        kind = "exit";
                        value = JS::Value(event.code);
                    }

                    return JS::call(vm, *handler, JS::js_undefined(), text(vm, kind), value);
                });
                if (event.kind == Event::Exit)
                    close(worker);
            }
        }

        void stop()
        {
            stopping = true;
            // Interrupt all first, then join. Child shutdown may itself join
            // nested workers; no owner loop is required to finish these joins.
            for (auto const& [id, worker] : workers)
                worker->channel->interrupt();
            for (auto const& [id, worker] : workers)
                close(*worker);
            if (child)
                child->stop();
        }
    };
}

GC::Ref<JS::Object> create_worker_host(JS::VM& vm, JS::Realm& realm, EventLoop& loop)
{
    auto service = std::make_shared<Service>(vm, realm, loop, current_child);
    if (service->child && !service->child->initialized)
        service->child->attach(vm, realm, loop);
    loop.on_shutdown([service] { service->stop(); });
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_direct_property(
        "isMainThread"_utf16, JS::Value(!service->child), JS::default_attributes);
    host->define_direct_property("threadId"_utf16,
        JS::Value(service->child ? service->child->channel->thread_id : 0), JS::default_attributes);
    host->define_direct_property("name"_utf16,
        text(vm, service->child ? service->child->channel->name : std::string {}),
        JS::default_attributes);
    host->define_direct_property("queueFull"_utf16, JS::Value(UV_ENOBUFS), JS::default_attributes);
    host->define_native_function(realm, "create"_utf16,
        GC::create_function(vm.heap(), GC::captures(shared_capture(service)),
            [](auto& captured, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& service = *captured;
                auto file = TRY(string(vm, vm.argument(0), 32768));
                auto name = TRY(string(vm, vm.argument(1), 256));
                if (!vm.argument(2).is_function() || file.empty()
                    || !std::filesystem::path(
                        std::u8string(reinterpret_cast<char8_t const*>(file.data()), file.size()))
                        .is_absolute())
                    return vm.throw_completion<JS::TypeError>("Invalid worker admission"_utf16);
                if (service.stopping)
                    return vm.throw_completion<JS::Error>("Worker host is closed"_utf16);
                if (active_workers.fetch_add(1) >= worker_limit) {
                    --active_workers;
                    return vm.throw_completion<JS::RangeError>("Native worker limit reached"_utf16);
                }

                auto channel = std::make_shared<Channel>();
                channel->thread_id = allocate_thread_id();
                if (!channel->thread_id || channel->thread_id > 0x7fffffff) {
                    --active_workers;
                    return vm.throw_completion<JS::RangeError>(
                        "Native worker identity exhausted"_utf16);
                }

                channel->name = std::move(name);
                OwnerSession session;
                session.worker = true;
                session.entry_path = std::filesystem::path(
                    std::u8string(reinterpret_cast<char8_t const*>(file.data()), file.size()));
                char executable[32768];
                size_t executable_size = sizeof(executable);
                auto status = uv_exepath(executable, &executable_size);
                if (status) {
                    --active_workers;
                    return vm.throw_completion<JS::Error>("Cannot resolve worker executable"_utf16);
                }

                session.argv = { std::string(executable, executable_size), file };
                if (auto inherited = worker_environment_snapshot()) {
                    session.environment = std::move(*inherited);
                } else {
                    uv_env_item_t* environment {};
                    int environment_count {};
                    status = uv_os_environ(&environment, &environment_count);
                    if (status) {
                        --active_workers;
                        return vm.throw_completion<JS::Error>(
                            "Cannot snapshot worker environment"_utf16);
                    }

                    for (int i = 0; i < environment_count; ++i)
                        session.environment.emplace(environment[i].name, environment[i].value);
                    uv_os_free_environ(environment, environment_count);
                }

                auto* worker = new Worker { service, channel->thread_id, channel,
                    GC::Root { vm.argument(2).as_function() },
                    GC::Root { service.loop.async_context().capture() }, {}, {}, false, false };
                status = uv_async_init(
                    &service.loop.native_loop(), &worker->wake, [](uv_async_t* handle) {
                        auto& worker = *static_cast<Worker*>(handle->data);
                        worker.owner.drain(worker);
                    });
                if (status) {
                    delete worker;
                    --active_workers;
                    return vm.throw_completion<JS::Error>("Cannot initialize worker wakeup"_utf16);
                }

                worker->wake.data = worker;
                channel->parent_wake = &worker->wake;
                service.workers.emplace(worker->id, worker);
                struct Start {
                    std::shared_ptr<Channel> channel;
                    OwnerSession session;
                };

                auto* start = new Start { channel, std::move(session) };
                uv_thread_options_t options {};
                options.flags = UV_THREAD_HAS_STACK_SIZE;
                options.stack_size = JS::VM::MINIMUM_THREAD_STACK_SIZE;
                status = uv_thread_create_ex(
                    &worker->thread, &options,
                    [](void* context) {
                        std::unique_ptr<Start> start(static_cast<Start*>(context));
                        run_child(std::move(start->channel), std::move(start->session));
                    },
                    start);
                if (status) {
                    delete start;
                    service.close(*worker);
                    return vm.throw_completion<JS::Error>("Cannot start worker thread"_utf16);
                }

                worker->started = true;
                return JS::Value(worker->id);
            }),
        3, JS::default_attributes);
    host->define_native_function(realm, "post"_utf16,
        GC::create_function(vm.heap(), GC::captures(shared_capture(service)),
            [](auto& service, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* worker = service->get(vm.argument(0));
                if (!worker || worker->closing || service->stopping)
                    return JS::Value(UV_EPIPE);
                auto bytes = TRY(payload(vm, service->realm, vm.argument(1)));
                std::lock_guard lock(worker->channel->mutex);
                auto& channel = *worker->channel;
                if (channel.closed || channel.port_closed || channel.finished || channel.stopping)
                    return JS::Value(UV_EPIPE);
                if (channel.incoming.size() >= count_limit
                    || channel.incoming_bytes + bytes.size() > queue_limit)
                    return JS::Value(UV_ENOBUFS);
                channel.incoming_bytes += bytes.size();
                channel.incoming.push_back(std::move(bytes));
                if (channel.child_wake)
                    uv_async_send(channel.child_wake);
                return JS::Value(0);
            }),
        2, JS::default_attributes);
    host->define_native_function(realm, "terminate"_utf16,
        GC::create_function(vm.heap(), GC::captures(shared_capture(service)),
            [](auto& service, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (auto* worker = service->get(vm.argument(0)); worker && !worker->closing)
                    worker->channel->interrupt();
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    host->define_native_function(realm, "ref"_utf16,
        GC::create_function(vm.heap(), GC::captures(shared_capture(service)),
            [](auto& service, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(1).is_boolean())
                    return vm.throw_completion<JS::TypeError>(
                        "Worker reference boolean required"_utf16);
                if (auto* worker = service->get(vm.argument(0)); worker && !worker->closing) {
                    auto* handle = reinterpret_cast<uv_handle_t*>(&worker->wake);
                    if (vm.argument(1).as_bool())
                        uv_ref(handle);
                    else
                        uv_unref(handle);
                }

                return JS::js_undefined();
            }),
        2, JS::default_attributes);
    host->define_native_function(realm, "parentSubscribe"_utf16,
        GC::create_function(vm.heap(), GC::captures(shared_capture(service)),
            [](auto& service, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!service->child || service->child->stopped || !vm.argument(0).is_function())
                    return vm.throw_completion<JS::TypeError>(
                        "Worker parent callback required"_utf16);
                service->child->callback = &vm.argument(0).as_function();
                uv_async_send(&service->child->wake);
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    host->define_native_function(realm, "parentPost"_utf16,
        GC::create_function(vm.heap(), GC::captures(shared_capture(service)),
            [](auto& service, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!service->child || service->child->stopped)
                    return JS::Value(UV_EPIPE);
                auto bytes = TRY(payload(vm, service->realm, vm.argument(0)));
                auto& channel = *service->child->channel;
                std::lock_guard lock(channel.mutex);
                if (channel.closed || channel.port_closed || channel.stopping)
                    return JS::Value(UV_EPIPE);
                if (channel.outgoing.size() >= count_limit
                    || channel.outgoing_bytes + bytes.size() > queue_limit)
                    return JS::Value(UV_ENOBUFS);
                channel.outgoing_bytes += bytes.size();
                channel.outgoing.push_back({ Event::Message, std::move(bytes), {}, {}, {}, 0 });
                if (channel.parent_wake)
                    uv_async_send(channel.parent_wake);
                return JS::Value(0);
            }),
        1, JS::default_attributes);
    host->define_native_function(realm, "parentClose"_utf16,
        GC::create_function(vm.heap(), GC::captures(shared_capture(service)),
            [](auto& service, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                if (service->child && !service->child->stopped) {
                    service->child->callback = nullptr;
                    uv_unref(reinterpret_cast<uv_handle_t*>(&service->child->wake));
                    std::lock_guard lock(service->child->channel->mutex);
                    service->child->channel->incoming.clear();
                    service->child->channel->incoming_bytes = 0;
                    service->child->channel->port_closed = true;
                }

                return JS::js_undefined();
            }),
        0, JS::default_attributes);
    host->define_native_function(realm, "parentRef"_utf16,
        GC::create_function(vm.heap(), GC::captures(shared_capture(service)),
            [](auto& service, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_boolean())
                    return vm.throw_completion<JS::TypeError>(
                        "Worker reference boolean required"_utf16);
                if (service->child && !service->child->stopped) {
                    auto* handle = reinterpret_cast<uv_handle_t*>(&service->child->wake);
                    if (vm.argument(0).as_bool())
                        uv_ref(handle);
                    else
                        uv_unref(handle);
                }

                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    return *host;
}
}
