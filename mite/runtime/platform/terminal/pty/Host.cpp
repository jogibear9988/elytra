/**
 * Owner-thread PTY lifecycle over a nonblocking master and a specific-PID reaper.
 * Polling never transfers GC values to another thread. Pause applies kernel
 * backpressure; writes own bounded byte snapshots until the kernel accepts them.
 */
#include "platform/terminal/pty/Host.h"
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include "core/NativeByteSource.h"
#include "platform/UV.h"
#include "platform/terminal/pty/Posix.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <memory>
#include <unordered_map>

namespace Mite {
namespace {
    constexpr size_t queue_limit = 1024 * 1024;

    JS::Value text(JS::VM& vm, std::string const& bytes)
    {
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8_with_replacement_character({ bytes.data(), bytes.size() }));
    }

    JS::Completion failure(JS::VM& vm, int status, char const* operation)
    {
        auto error = JS::Error::create(vm.realm(),
            Utf16String::from_utf8_with_replacement_character(
                StringView(uv_strerror(status), strlen(uv_strerror(status)))));
        error->define_direct_property("code"_utf16,
            text(vm, status == UV_ENOSYS ? "ERR_MITE_UNSUPPORTED_API" : uv_err_name(status)),
            JS::default_attributes);
        error->define_direct_property("syscall"_utf16, text(vm, operation), JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::ThrowCompletionOr<std::string> string(JS::VM& vm, JS::Value value)
    {
        if (!value.is_string())
            return failure(vm, UV_EINVAL, "pty argument");
        auto converted = value.as_string().utf16_string_view().to_byte_string();
        if (converted.is_error())
            return failure(vm, UV_EINVAL, "pty argument");
        auto bytes = converted.release_value();
        if (bytes.length() > 65536 || bytes.contains('\0'))
            return failure(vm, UV_EINVAL, "pty argument");
        return std::string(bytes.characters(), bytes.length());
    }

    JS::ThrowCompletionOr<int> integer(JS::VM& vm, JS::Value value, int minimum, int maximum)
    {
        if (!value.is_number() || !std::isfinite(value.as_double()) || value.as_double() < minimum
            || value.as_double() > maximum || std::trunc(value.as_double()) != value.as_double())
            return failure(vm, UV_EINVAL, "pty argument");
        return static_cast<int>(value.as_double());
    }

    JS::ThrowCompletionOr<std::vector<std::string>> strings(
        JS::VM& vm, JS::Value value, size_t maximum, size_t& budget, bool environment)
    {
        if (!value.is_object() || !JS::Array::is_engine_class_of(value.as_object()))
            return failure(vm, UV_EINVAL, "pty arguments");
        GC::Root array { value.as_object() };
        auto length = TRY(array->get(vm.names.length)).as_double();
        if (length > maximum)
            return failure(vm, UV_ENOBUFS, "pty arguments");
        std::vector<std::string> result;
        for (u32 index = 0; index < length; ++index) {
            auto entry = TRY(string(vm, TRY(array->get(index))));
            if (entry.size() + 1 > queue_limit - budget)
                return failure(vm, UV_ENOBUFS, "pty arguments");
            if (environment && (entry.find('=') == std::string::npos || entry.front() == '='))
                return failure(vm, UV_EINVAL, "pty environment");
            budget += entry.size() + 1;
            result.push_back(std::move(entry));
        }

        return result;
    }

    struct Service;

    struct Terminal {
        Service& owner;
        uint32_t id;
        Pty::Process process;
        GC::Root<JS::FunctionObject> callback;
        GC::Root<AsyncCarrier> context;
        uv_poll_t poll {};
        uv_timer_t timer {};
        size_t handles {}, queued {}, offset {};
        std::deque<std::vector<char>> writes;
        bool poll_initialized {}, timer_initialized {}, paused {}, eof {}, reaped {}, closing {},
            destroying {}, killed {}, exit_emitted {};
        int exit_code {}, exit_signal {};
        std::chrono::steady_clock::time_point drain_deadline {}, kill_deadline {};
    };

    struct Service {
        JS::VM& vm;
        JS::Realm& realm;
        EventLoop& loop;
        std::unordered_map<uint32_t, std::unique_ptr<Terminal>> terminals;
        uint32_t next_id { 1 };
        bool stopping {};

        void visit_capture_edges(GC::Cell::Visitor& visitor) const
        {
            if (stopping)
                return;
            visitor.visit(realm);
            for (auto const& entry : terminals) {
                visitor.visit(entry.second->callback.ptr());
                visitor.visit(entry.second->context.ptr());
            }
        }

        Terminal* get(JS::Value value)
        {
            if (!value.is_number() || value.as_double() < 1 || value.as_double() > 0x7fffffff
                || std::trunc(value.as_double()) != value.as_double())
                return nullptr;
            auto found = terminals.find(static_cast<uint32_t>(value.as_double()));
            return found == terminals.end() || found->second->closing ? nullptr
                                                                      : found->second.get();
        }

        void emit(Terminal& terminal, char const* kind, JS::Value value, int signal = 0)
        {
            if (stopping || !terminal.callback)
                return;
            GC::Root callback { *terminal.callback };
            GC::RootVector<JS::Value> retained;
            retained.append(value);
            loop.dispatch([&] {
                AsyncContext::Scope context(loop.async_context(), terminal.context.ptr());
                return JS::call(vm, *callback, JS::js_undefined(), text(vm, kind), retained[0],
                    JS::Value(signal));
            });
        }

        static void released(uv_handle_t* handle)
        {
            auto& terminal = *static_cast<Terminal*>(handle->data);
            if (--terminal.handles == 0)
                terminal.owner.terminals.erase(terminal.id);
        }

        void close_poll(Terminal& terminal)
        {
            if (terminal.poll_initialized
                && !uv_is_closing(reinterpret_cast<uv_handle_t*>(&terminal.poll))) {
                uv_poll_stop(&terminal.poll);
                // Stop monitoring before closing: an fd reused by a later spawn
                // must never inherit readiness from this terminal.
                uv_close(reinterpret_cast<uv_handle_t*>(&terminal.poll), released);
            }

            Pty::close_master(terminal.process);
        }

        void retire(Terminal& terminal)
        {
            if (terminal.closing)
                return;
            terminal.closing = true;
            terminal.callback = nullptr;
            terminal.context = nullptr;
            terminal.writes.clear();
            terminal.queued = 0;
            close_poll(terminal);
            if (terminal.timer_initialized
                && !uv_is_closing(reinterpret_cast<uv_handle_t*>(&terminal.timer))) {
                uv_timer_stop(&terminal.timer);
                uv_close(reinterpret_cast<uv_handle_t*>(&terminal.timer), released);
            }
        }

        void finish(Terminal& terminal)
        {
            if (terminal.closing || terminal.exit_emitted || !terminal.reaped || !terminal.eof)
                return;
            terminal.exit_emitted = true;
            emit(terminal, "exit", JS::Value(terminal.exit_code), terminal.exit_signal);
            retire(terminal);
        }

        void destroy(Terminal& terminal)
        {
            if (terminal.destroying || terminal.closing)
                return;
            terminal.destroying = true;
            terminal.eof = true;
            close_poll(terminal);
            terminal.writes.clear();
            terminal.queued = 0;
            if (!terminal.reaped) {
                (void)Pty::signal(terminal.process, "SIGHUP");
                terminal.kill_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            }

            finish(terminal);
        }

        void error(Terminal& terminal, int status)
        {
            auto completion = failure(vm, status, "pty IO");
            emit(terminal, "error", completion.value());
            if (!terminal.closing)
                destroy(terminal);
        }

        int monitor(Terminal& terminal, bool startup = false)
        {
            if (terminal.closing || terminal.eof)
                return 0;
            int events = terminal.paused ? 0 : UV_READABLE;
            if (!terminal.writes.empty())
                events |= UV_WRITABLE;
            if (!events) {
                uv_poll_stop(&terminal.poll);
                return 0;
            }

            auto status = uv_poll_start(
                &terminal.poll, events, [](uv_poll_t* handle, int status, int events) {
                    auto& terminal = *static_cast<Terminal*>(handle->data);
                    auto& owner = terminal.owner;
                    if (terminal.closing)
                        return;
                    if (status) {
                        owner.error(terminal, status);
                        return;
                    }

                    if (events & UV_WRITABLE)
                        owner.flush(terminal);
                    if (!terminal.closing && !terminal.eof && (events & UV_READABLE))
                        owner.drain(terminal);
                });
            if (status && !startup)
                error(terminal, status);
            return status;
        }

        void drain(Terminal& terminal)
        {
            if (terminal.paused || terminal.closing || terminal.eof)
                return;
            std::array<char, 65536> buffer;
            size_t count = 0;
            auto status = Pty::read(terminal.process, buffer.data(), buffer.size(), count);
            if (status == UV_EAGAIN)
                return;
            if (status == UV_EOF) {
                terminal.eof = true;
                close_poll(terminal);
                finish(terminal);
                return;
            }

            if (status) {
                error(terminal, status);
                return;
            }

            loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                if (stopping || !terminal.callback)
                    return JS::js_undefined();
                AsyncContext::Scope context(loop.async_context(), terminal.context.ptr());
                GC::Root bytes { TRY(JS::Uint8Array::create(realm, count)) };
                bytes->viewed_array_buffer()->overwrite(0, buffer.data(), count);
                return JS::call(vm, *terminal.callback, JS::js_undefined(), text(vm, "data"),
                    bytes.ptr(), JS::Value(0));
            });
        }

        void flush(Terminal& terminal)
        {
            // Limit work in a readiness callback; queued snapshots remain owned.
            size_t budget = 65536;
            while (!terminal.closing && !terminal.eof && !terminal.writes.empty() && budget) {
                auto& bytes = terminal.writes.front();
                auto size = std::min(budget, bytes.size() - terminal.offset);
                size_t count = 0;
                auto status
                    = Pty::write(terminal.process, bytes.data() + terminal.offset, size, count);
                if (status == UV_EAGAIN)
                    break;
                if (status) {
                    error(terminal, status);
                    return;
                }

                if (!count)
                    break;
                budget -= count;
                terminal.offset += count;
                terminal.queued -= count;
                if (terminal.offset == bytes.size()) {
                    terminal.writes.pop_front();
                    terminal.offset = 0;
                }
            }

            monitor(terminal);
        }

        void tick(Terminal& terminal)
        {
            if (terminal.closing)
                return;
            auto now = std::chrono::steady_clock::now();
            if (!terminal.reaped) {
                bool exited = false;
                auto status
                    = Pty::reap(terminal.process, exited, terminal.exit_code, terminal.exit_signal);
                if (status) {
                    error(terminal, status);
                    return;
                }

                if (exited) {
                    terminal.reaped = true;
                    // A reaped PID can be reused: never signal it again.
                    terminal.process.pid = -1;
                    terminal.drain_deadline = now + std::chrono::milliseconds(200);
                } else if (terminal.destroying && !terminal.killed
                    && now >= terminal.kill_deadline) {
                    (void)Pty::signal(terminal.process, "SIGKILL");
                    terminal.killed = true;
                }
            }

            if (terminal.reaped) {
                drain(terminal);
                if (terminal.closing)
                    return;
                if (!terminal.eof && now >= terminal.drain_deadline) {
                    // Same bounded close grace as node-pty's Unix backend when
                    // inherited slave descriptors prevent EOF (also when paused).
                    terminal.eof = true;
                    close_poll(terminal);
                }

                finish(terminal);
            }
        }

        void stop()
        {
            stopping = true;
            for (auto const& entry : terminals) {
                auto& terminal = *entry.second;
                terminal.callback = nullptr;
                terminal.context = nullptr;
                close_poll(terminal);
                Pty::terminate_and_reap(terminal.process);
                retire(terminal);
            }
        }
    };
}

GC::Ref<JS::Object> create_pty_host(JS::Realm& realm, EventLoop& loop)
{
    auto service = std::make_shared<Service>(realm.vm(), realm, loop);
    loop.on_shutdown([service] { service->stop(); });
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_direct_property(
        "available"_utf16, JS::Value(Pty::available()), JS::default_attributes);
    host->define_native_function(realm, "spawn"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(service)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& service = *owner;
                if (service.stopping || !Pty::available())
                    return failure(vm, UV_ENOSYS, "pty spawn");
                if (service.terminals.size() >= 16 || service.next_id > 0x7fffffff)
                    return failure(vm, UV_ENOBUFS, "pty spawn");
                if (!vm.argument(0).is_object() || !vm.argument(1).is_function())
                    return failure(vm, UV_EINVAL, "pty spawn");
                GC::Root spec { vm.argument(0).as_object() };
                GC::Root callback { vm.argument(1).as_function() };
                Pty::Spawn options;
                options.file = TRY(string(vm, TRY(spec->get("file"_utf16))));
                options.cwd = TRY(string(vm, TRY(spec->get("cwd"_utf16))));
                options.cols = TRY(integer(vm, TRY(spec->get("cols"_utf16)), 1, 65535));
                options.rows = TRY(integer(vm, TRY(spec->get("rows"_utf16)), 1, 65535));
                size_t budget = options.file.size() + options.cwd.size() + 2;
                options.args = TRY(strings(vm, TRY(spec->get("args"_utf16)), 1024, budget, false));
                options.env = TRY(strings(vm, TRY(spec->get("env"_utf16)), 512, budget, true));
                if (options.file.empty() || options.cwd.empty())
                    return failure(vm, UV_EINVAL, "pty spawn");
                // Spec getters may re-enter spawn; revalidate after the complete snapshot.
                if (service.stopping || service.terminals.size() >= 16
                    || service.next_id > 0x7fffffff)
                    return failure(vm, UV_ENOBUFS, "pty spawn");
                std::array<char, 65536> executable;
                size_t length = executable.size();
                auto status = uv_exepath(executable.data(), &length);
                if (status)
                    return failure(vm, status, "pty spawn");
                options.executable.assign(executable.data(), length);
                auto terminal = std::make_unique<Terminal>(service, service.next_id++);
                status = Pty::spawn(options, terminal->process);
                if (status)
                    return failure(vm, status, "pty spawn");
                terminal->callback = callback.ptr();
                terminal->context = service.loop.async_context().capture();
                auto id = terminal->id;
                auto* native = terminal.get();
                service.terminals.emplace(id, std::move(terminal));
                status = uv_poll_init(
                    &service.loop.native_loop(), &native->poll, native->process.master);
                if (!status) {
                    native->poll_initialized = true;
                    ++native->handles;
                    native->poll.data = native;
                }

                if (!status)
                    status = uv_timer_init(&service.loop.native_loop(), &native->timer);
                if (!status) {
                    native->timer_initialized = true;
                    ++native->handles;
                    native->timer.data = native;
                }

                if (!status)
                    status = uv_timer_start(
                        &native->timer,
                        [](uv_timer_t* timer) {
                            auto& terminal = *static_cast<Terminal*>(timer->data);
                            terminal.owner.tick(terminal);
                        },
                        10, 10);
                if (status) {
                    service.close_poll(*native);
                    Pty::terminate_and_reap(native->process);
                    service.retire(*native);
                    if (!native->handles)
                        service.terminals.erase(id);
                    return failure(vm, status, "pty spawn");
                }

                status = service.monitor(*native, true);
                if (status) {
                    service.close_poll(*native);
                    Pty::terminate_and_reap(native->process);
                    service.retire(*native);
                    return failure(vm, status, "pty spawn");
                }

                GC::Root result { JS::Object::create(service.realm, nullptr) };
                result->define_direct_property("id"_utf16, JS::Value(id), JS::default_attributes);
                result->define_direct_property(
                    "pid"_utf16, JS::Value(native->process.pid), JS::default_attributes);
                result->define_direct_property(
                    "pty"_utf16, text(vm, native->process.name), JS::default_attributes);
                return result.ptr();
            }),
        2, JS::default_attributes);
    host->define_native_function(realm, "write"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(service)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* terminal = owner->get(vm.argument(0));
                if (!terminal || terminal->eof || terminal->reaped)
                    return failure(vm, UV_EBADF, "pty write");
                auto bytes = TRY(native_byte_source(vm, vm.argument(1), false));
                if (bytes.length > queue_limit - terminal->queued)
                    return failure(vm, UV_ENOBUFS, "pty write");
                if (bytes.length) {
                    auto* data = reinterpret_cast<char const*>(bytes.buffer->data_at(bytes.offset));
                    terminal->writes.emplace_back(data, data + bytes.length);
                    terminal->queued += bytes.length;
                    owner->flush(*terminal);
                }

                return JS::js_undefined();
            }),
        2, JS::default_attributes);
    host->define_native_function(realm, "resize"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(service)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* terminal = owner->get(vm.argument(0));
                if (!terminal || terminal->eof)
                    return failure(vm, UV_EBADF, "pty resize");
                auto cols = TRY(integer(vm, vm.argument(1), 1, 65535));
                auto rows = TRY(integer(vm, vm.argument(2), 1, 65535));
                auto width = TRY(integer(vm, vm.argument(3), 0, 65535));
                auto height = TRY(integer(vm, vm.argument(4), 0, 65535));
                auto status = Pty::resize(terminal->process, cols, rows, width, height);
                if (status)
                    return failure(vm, status, "pty resize");
                return JS::js_undefined();
            }),
        5, JS::default_attributes);
    host->define_native_function(realm, "pause"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(service)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* terminal = owner->get(vm.argument(0));
                if (!terminal || !vm.argument(1).is_boolean())
                    return failure(vm, UV_EBADF, "pty pause");
                terminal->paused = vm.argument(1).as_bool();
                owner->monitor(*terminal);
                return JS::js_undefined();
            }),
        2, JS::default_attributes);
    host->define_native_function(realm, "kill"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(service)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* terminal = owner->get(vm.argument(0));
                if (!terminal || terminal->reaped)
                    return failure(vm, UV_ESRCH, "pty kill");
                auto name = TRY(string(vm, vm.argument(1)));
                auto status = Pty::signal(terminal->process, name);
                if (status)
                    return failure(vm, status, "pty kill");
                return JS::js_undefined();
            }),
        2, JS::default_attributes);
    host->define_native_function(realm, "destroy"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(service)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (auto* terminal = owner->get(vm.argument(0)))
                    owner->destroy(*terminal);
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    host->define_native_function(realm, "process"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(service)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* terminal = owner->get(vm.argument(0));
                return text(vm,
                    terminal && !terminal->eof ? Pty::foreground_process(terminal->process) : "");
            }),
        1, JS::default_attributes);
    return *host;
}
}
