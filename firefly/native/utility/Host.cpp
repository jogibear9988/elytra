/** Owned libuv subprocesses and bounded binary frames, independent of the renderer transport.
 * Every OS callback executes on Mite's owner thread and enters JS through dispatch().
 * The parent kills and reaps owned children at teardown; no JS runs after shutdown.
 */
#include "Host.h"
#include "core/EventLoop.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <signal.h>
#include <string>
#include <unordered_map>
#include <uv.h>
#include <vector>

namespace Firefly::Utility {
namespace {
    constexpr size_t frame_limit = 128 * 1024;
    constexpr size_t write_count_limit = 16;
    constexpr size_t write_bytes_limit = 2 * 1024 * 1024;
    constexpr size_t child_limit = 32;

    JS::Value text(JS::VM& vm, char const* value)
    {
        return JS::PrimitiveString::create(vm,
            Utf16String::from_utf8_with_replacement_character(StringView(value, strlen(value))));
    }

    JS::ThrowCompletionOr<std::string> string(JS::VM& vm, JS::Value value)
    {
        if (!value.is_string())
            return vm.throw_completion<JS::TypeError>("Invalid utility process string"sv);
        auto bytes = MUST(value.as_string().utf16_string_view().to_byte_string());
        if (bytes.contains('\0'))
            return vm.throw_completion<JS::TypeError>("NUL in utility process string"sv);
        return std::string(bytes.characters(), bytes.length());
    }

    JS::ThrowCompletionOr<std::vector<std::string>> strings(JS::VM& vm, JS::Value value)
    {
        if (!value.is_object() || !is<JS::Array>(value.as_object()))
            return vm.throw_completion<JS::TypeError>("Invalid utility process arguments"sv);
        auto& array = value.as_object();
        auto size = TRY(TRY(array.get("length"_utf16)).to_u32(vm));
        if (size > 65536)
            return vm.throw_completion<JS::RangeError>("Too many utility process arguments"sv);
        std::vector<std::string> result;
        for (uint32_t i = 0; i < size; ++i)
            result.push_back(TRY(string(vm, TRY(array.get(i)))));
        return result;
    }

    std::vector<char*> pointers(std::vector<std::string>& values)
    {
        std::vector<char*> result;
        for (auto& value : values)
            result.push_back(value.data());
        result.push_back(nullptr);
        return result;
    }

    struct Service;
    struct Endpoint;

    struct Pipe {
        Endpoint* endpoint {};
        int index {};
        uv_pipe_t handle {};
        bool initialized {};

        uv_stream_t* stream()
        {
            return reinterpret_cast<uv_stream_t*>(&handle);
        }

        uv_handle_t* base()
        {
            return reinterpret_cast<uv_handle_t*>(&handle);
        }
    };

    struct Endpoint {
        Endpoint(Service& service, uint64_t identifier, GC::Root<JS::FunctionObject> handler)
            : owner(service)
            , id(identifier)
            , callback(handler)
        {
        }

        Service& owner;
        uint64_t id;
        GC::Root<JS::FunctionObject> callback;
        std::array<Pipe, 4> pipes {};
        uv_process_t process {};
        uv_timer_t startup {};
        size_t handles {}, writes {}, write_bytes {};
        bool process_initialized {}, timer_initialized {}, started {}, announced {}, exited {},
            killed {}, channel_closed {};
        std::vector<char> incoming;
    };

    struct Service {
        JS::VM& vm;
        JS::Realm& realm;
        Mite::EventLoop& loop;
        bool child_side {}, stopped {};
        uint64_t next_id { 1 };
        std::unordered_map<uint64_t, Endpoint*> endpoints;

        ~Service()
        {
            VERIFY(endpoints.empty());
        }

        Endpoint* get(JS::Value id)
        {
            if (!id.is_number() || !std::isfinite(id.as_double()) || id.as_double() < 1
                || id.as_double() >= 9007199254740992.0
                || std::trunc(id.as_double()) != id.as_double())
                return nullptr;
            auto found = endpoints.find(static_cast<uint64_t>(id.as_double()));
            return found == endpoints.end() ? nullptr : found->second;
        }

        void emit(Endpoint& endpoint, char const* kind, JS::Value value = JS::js_undefined())
        {
            if (stopped || !endpoint.callback)
                return;
            // Root independently of reentrant close/shutdown from the callback.
            GC::Root callback { endpoint.callback };
            loop.dispatch([&] {
                if (child_side)
                    return JS::call(vm, *callback, JS::js_undefined(), text(vm, kind), value);
                return JS::call(vm, *callback, JS::js_undefined(), text(vm, kind),
                    JS::Value(static_cast<double>(endpoint.id)), value);
            });
        }

        void bytes(Endpoint& endpoint, char const* kind, char const* data, size_t size)
        {
            if (stopped || !endpoint.callback)
                return;
            GC::Root callback { endpoint.callback };
            loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                GC::Root array { TRY(JS::Uint8Array::create(realm, size)) };
                if (size)
                    array->viewed_array_buffer()->overwrite(0, data, size);
                if (child_side)
                    return JS::call(
                        vm, *callback, JS::js_undefined(), text(vm, kind), JS::Value(array.ptr()));
                return JS::call(vm, *callback, JS::js_undefined(), text(vm, kind),
                    JS::Value(static_cast<double>(endpoint.id)), JS::Value(array.ptr()));
            });
        }

        void released(Endpoint& endpoint)
        {
            VERIFY(endpoint.handles);
            if (--endpoint.handles == 0) {
                endpoints.erase(endpoint.id);
                delete &endpoint;
            }
        }

        void close(Pipe& pipe)
        {
            if (!pipe.initialized || uv_is_closing(pipe.base()))
                return;
            uv_read_stop(pipe.stream());
            uv_close(pipe.base(), [](uv_handle_t* handle) {
                auto& pipe = *static_cast<Pipe*>(handle->data);
                pipe.endpoint->owner.released(*pipe.endpoint);
            });
        }

        void close_startup(Endpoint& endpoint)
        {
            auto* handle = reinterpret_cast<uv_handle_t*>(&endpoint.startup);
            if (!endpoint.timer_initialized || uv_is_closing(handle))
                return;
            uv_timer_stop(&endpoint.startup);
            uv_close(handle, [](uv_handle_t* handle) {
                auto& endpoint = *static_cast<Endpoint*>(handle->data);
                endpoint.owner.released(endpoint);
            });
        }

        // A process can be reaped before its startup timer runs. Announce exactly
        // once from either path, before any exit notification, and begin draining
        // inherited pipes even when the child has already exited.
        void announce(Endpoint& endpoint)
        {
            if (endpoint.announced)
                return;
            endpoint.announced = true;
            close_startup(endpoint);
            emit(endpoint, "spawn", JS::Value(static_cast<double>(endpoint.process.pid)));
            for (auto& pipe : endpoint.pipes)
                if (pipe.initialized && !uv_is_closing(pipe.base()) && !stopped) {
                    auto status = read(pipe);
                    if (status)
                        error(endpoint, status);
                }
        }

        void close_channel(Endpoint& endpoint)
        {
            if (endpoint.channel_closed)
                return;
            endpoint.channel_closed = true;
            endpoint.incoming.clear();
            close(endpoint.pipes[3]);
            emit(endpoint, "close");
        }

        void error(Endpoint& endpoint, int status)
        {
            emit(endpoint, "error", text(vm, uv_strerror(status)));
            close_channel(endpoint);
        }

        int initialize(Endpoint& endpoint, int index)
        {
            auto& pipe = endpoint.pipes[static_cast<size_t>(index)];
            pipe.endpoint = &endpoint;
            pipe.index = index;
            auto status = uv_pipe_init(&loop.native_loop(), &pipe.handle, 0);
            if (!status) {
                pipe.initialized = true;
                pipe.handle.data = &pipe;
                ++endpoint.handles;
            }

            return status;
        }

        int read(Pipe& pipe)
        {
            return uv_read_start(
                pipe.stream(),
                [](uv_handle_t*, size_t size, uv_buf_t* buffer) {
                    size = std::min(size, size_t { 65536 });
                    buffer->base = static_cast<char*>(malloc(size));
                    buffer->len = buffer->base ? size : 0;
                },
                [](uv_stream_t* stream, ssize_t count, uv_buf_t const* buffer) {
                    auto& pipe = *static_cast<Pipe*>(stream->data);
                    auto& endpoint = *pipe.endpoint;
                    auto& owner = endpoint.owner;
                    if (count > 0 && !owner.stopped) {
                        if (pipe.index == 3)
                            owner.receive(endpoint, buffer->base, static_cast<size_t>(count));
                        else
                            owner.bytes(endpoint, pipe.index == 1 ? "stdout" : "stderr",
                                buffer->base, static_cast<size_t>(count));
                    } else if (count < 0) {
                        if (pipe.index == 3) {
                            if (count != UV_EOF || !endpoint.incoming.empty())
                                owner.error(endpoint,
                                    count == UV_EOF ? UV_EPROTO : static_cast<int>(count));
                            else
                                owner.close_channel(endpoint);
                        } else {
                            owner.emit(endpoint, pipe.index == 1 ? "stdoutEnd" : "stderrEnd");
                            owner.close(pipe);
                        }
                    }

                    free(buffer->base);
                });
        }

        void receive(Endpoint& endpoint, char const* data, size_t size)
        {
            // At most one incomplete frame plus one read buffer is retained. Copy each
            // completed payload before invoking JS, which may close this channel.
            endpoint.incoming.insert(endpoint.incoming.end(), data, data + size);
            while (!stopped && !endpoint.channel_closed && endpoint.incoming.size() >= 4) {
                auto* p = reinterpret_cast<unsigned char const*>(endpoint.incoming.data());
                size_t length
                    = (size_t(p[0]) << 24) | (size_t(p[1]) << 16) | (size_t(p[2]) << 8) | p[3];
                if (length > frame_limit) {
                    error(endpoint, UV_EMSGSIZE);
                    return;
                }

                if (endpoint.incoming.size() < length + 4)
                    return;
                std::vector<char> payload(
                    endpoint.incoming.begin() + 4, endpoint.incoming.begin() + 4 + length);
                endpoint.incoming.erase(
                    endpoint.incoming.begin(), endpoint.incoming.begin() + 4 + length);
                bytes(endpoint, child_side ? "message" : "data", payload.data(), payload.size());
            }
        }

        JS::ThrowCompletionOr<JS::Value> send(JS::VM& vm, Endpoint* endpoint, JS::Value value)
        {
            if (stopped || !endpoint || endpoint->channel_closed)
                return JS::Value(UV_EPIPE);
            if (!value.is_object() || !is<JS::Uint8Array>(value.as_object()))
                return vm.throw_completion<JS::TypeError>("Utility channel requires Uint8Array"sv);
            auto& array = static_cast<JS::Uint8Array&>(value.as_object());
            auto witness = JS::make_typed_array_with_buffer_witness_record(
                array, JS::ArrayBuffer::Order::SeqCst);
            if (JS::is_typed_array_out_of_bounds(witness))
                return vm.throw_completion<JS::TypeError>("Detached utility message"sv);
            auto size = JS::typed_array_byte_length(witness);
            if (size > frame_limit)
                return JS::Value(UV_EMSGSIZE);
            if (endpoint->writes >= write_count_limit
                || endpoint->write_bytes + size + 4 > write_bytes_limit)
                return JS::Value(UV_ENOBUFS);

            struct Write {
                Endpoint& endpoint;
                uv_write_t request {};
                std::vector<char> bytes;
            };

            auto* write = new Write { *endpoint, {}, std::vector<char>(size + 4) };
            for (size_t i = 0; i < 4; ++i)
                write->bytes[i] = static_cast<char>(size >> ((3 - i) * 8));
            if (size)
                memcpy(write->bytes.data() + 4,
                    array.viewed_array_buffer()->data_at(array.byte_offset()), size);
            write->request.data = write;
            auto buffer
                = uv_buf_init(write->bytes.data(), static_cast<unsigned int>(write->bytes.size()));
            auto status = uv_write(&write->request, endpoint->pipes[3].stream(), &buffer, 1,
                [](uv_write_t* request, int status) {
                    auto* write = static_cast<Write*>(request->data);
                    auto& endpoint = write->endpoint;
                    --endpoint.writes;
                    endpoint.write_bytes -= write->bytes.size();
                    delete write;
                    if (status && !endpoint.owner.stopped && !endpoint.channel_closed)
                        endpoint.owner.error(endpoint, status);
                });
            if (status)
                delete write;
            else {
                ++endpoint->writes;
                endpoint->write_bytes += size + 4;
            }

            return JS::Value(status);
        }

        void shutdown()
        {
            if (stopped)
                return;
            stopped = true;
            for (auto& [id, endpoint] : endpoints) {
                endpoint->callback = nullptr;
                endpoint->channel_closed = true;
                close_startup(*endpoint);
                for (auto& pipe : endpoint->pipes)
                    close(pipe);
                if (endpoint->started && !endpoint->exited)
                    uv_process_kill(&endpoint->process, SIGKILL);
            }
        }
    };

    std::shared_ptr<Service> service(
        JS::VM& vm, JS::Realm& realm, Mite::EventLoop& loop, bool child)
    {
        auto result = std::make_shared<Service>(vm, realm, loop, child);
        loop.on_shutdown([result] { result->shutdown(); });
        return result;
    }
}

GC::Ref<JS::Object> create_host(
    JS::VM& vm, JS::Realm& realm, Mite::EventLoop& loop, bool electron_compat)
{
    auto owner = service(vm, realm, loop, false);
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_native_function(
        realm, "spawn"_utf16,
        [owner, &realm, electron_compat](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (owner->stopped)
                return vm.throw_completion<JS::Error>("Utility service stopped"sv);
            if (!vm.argument(1).is_function())
                return vm.throw_completion<JS::TypeError>("Utility callback must be callable"sv);
            auto spec = TRY(vm.argument(0).to_object(vm));
            std::array<char, 32768> executable {};
            size_t executable_size = executable.size();
            if (uv_exepath(executable.data(), &executable_size))
                return vm.throw_completion<JS::Error>("Cannot resolve utility executable"sv);
            std::string file(executable.data(), executable_size);
            auto entry = TRY(string(vm, TRY(spec->get("entry"_utf16))));
            auto args = TRY(strings(vm, TRY(spec->get("args"_utf16))));
            auto env = TRY(strings(vm, TRY(spec->get("env"_utf16))));
            auto cwd = TRY(string(vm, TRY(spec->get("cwd"_utf16))));
            auto stdio = TRY(TRY(spec->get("stdio"_utf16)).to_object(vm));
            std::array<std::string, 3> modes;
            for (size_t i = 0; i < 3; ++i) {
                modes[i] = TRY(string(vm, TRY(stdio->get(i))));
                if ((i == 0 && modes[i] != "ignore")
                    || (i != 0 && modes[i] != "ignore" && modes[i] != "pipe"
                        && modes[i] != "inherit"))
                    return vm.throw_completion<JS::TypeError>("Unsupported utility stdio mode"sv);
            }
            // Recheck after accessors: no user code executes between this check and spawn.
            if (owner->stopped || owner->endpoints.size() >= child_limit || file.empty()
                || entry.empty())
                return vm.throw_completion<JS::Error>(
                    "Utility process unavailable or limit exceeded"sv);
            args.insert(args.begin(),
                { file, electron_compat ? "--firefly-utility-electron" : "--firefly-utility",
                    entry });
            auto arguments = pointers(args), environment = pointers(env);
            auto* endpoint = new Endpoint { *owner, owner->next_id++,
                GC::Root { vm.argument(1).as_function() } };
            owner->endpoints.emplace(endpoint->id, endpoint);
            std::array<uv_stdio_container_t, 4> handles {};
            int status = 0;
            for (int i = 0; i < 4; ++i) {
                if (i == 3 || modes[static_cast<size_t>(i)] == "pipe") {
                    status = owner->initialize(*endpoint, i);
                    if (status)
                        break;
                    handles[i].flags = static_cast<uv_stdio_flags>(
                        UV_CREATE_PIPE | UV_WRITABLE_PIPE | (i == 3 ? UV_READABLE_PIPE : 0));
                    handles[i].data.stream = endpoint->pipes[i].stream();
                } else if (modes[static_cast<size_t>(i)] == "inherit") {
                    handles[i].flags = UV_INHERIT_FD;
                    handles[i].data.fd = i;
                } else
                    handles[i].flags = UV_IGNORE;
            }

            if (!status) {
                uv_process_options_t options {};
                options.file = file.c_str();
                options.args = arguments.data();
                options.env = environment.data();
                options.cwd = cwd.empty() ? nullptr : cwd.c_str();
                options.stdio = handles.data();
                options.stdio_count = 4;
                options.flags = UV_PROCESS_WINDOWS_HIDE;
                options.exit_cb = [](uv_process_t* process, int64_t code, int signal) {
                    auto& endpoint = *static_cast<Endpoint*>(process->data);
                    endpoint.exited = true;
                    endpoint.owner.announce(endpoint);
                    endpoint.owner.emit(
                        endpoint, "exit", JS::Value(signal ? 0 : static_cast<double>(code)));
                    uv_close(reinterpret_cast<uv_handle_t*>(process), [](uv_handle_t* handle) {
                        auto& endpoint = *static_cast<Endpoint*>(handle->data);
                        endpoint.owner.released(endpoint);
                    });
                };

                status = uv_spawn(&owner->loop.native_loop(), &endpoint->process, &options);
                endpoint->process.data = endpoint;
                endpoint->process_initialized = true;
                ++endpoint->handles;
                endpoint->started = !status;
            }

            auto id = endpoint->id;
            auto pid = endpoint->started ? endpoint->process.pid : 0;
            if (!status) {
                status = uv_timer_init(&owner->loop.native_loop(), &endpoint->startup);
                if (!status) {
                    endpoint->timer_initialized = true;
                    ++endpoint->handles;
                    endpoint->startup.data = endpoint;
                    status = uv_timer_start(
                        &endpoint->startup,
                        [](uv_timer_t* timer) {
                            auto& endpoint = *static_cast<Endpoint*>(timer->data);
                            endpoint.owner.announce(endpoint);
                        },
                        0, 0);
                }
            }

            if (status) {
                endpoint->channel_closed = true;
                endpoint->callback = nullptr;
                owner->close_startup(*endpoint);
                for (auto& pipe : endpoint->pipes)
                    owner->close(pipe);
                if (endpoint->started)
                    uv_process_kill(&endpoint->process, SIGKILL);
                else if (endpoint->process_initialized)
                    uv_close(reinterpret_cast<uv_handle_t*>(&endpoint->process),
                        [](uv_handle_t* handle) {
                            auto& endpoint = *static_cast<Endpoint*>(handle->data);
                            endpoint.owner.released(endpoint);
                        });
                if (!endpoint->handles) {
                    owner->endpoints.erase(id);
                    delete endpoint;
                }
            }

            GC::Root result { JS::Object::create(realm, nullptr) };
            result->define_direct_property(
                "id"_utf16, JS::Value(static_cast<double>(id)), JS::default_attributes);
            result->define_direct_property(
                "pid"_utf16, JS::Value(static_cast<double>(pid)), JS::default_attributes);
            result->define_direct_property(
                "status"_utf16, JS::Value(status), JS::default_attributes);
            return result.ptr();
        },
        2, JS::default_attributes);
    host->define_native_function(
        realm, "send"_utf16,
        [owner](JS::VM& vm) { return owner->send(vm, owner->get(vm.argument(0)), vm.argument(1)); },
        2, JS::default_attributes);
    host->define_native_function(
        realm, "kill"_utf16,
        [owner](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* endpoint = owner->get(vm.argument(0));
            if (owner->stopped || !endpoint || !endpoint->started || endpoint->exited
                || endpoint->killed)
                return JS::Value(false);
            endpoint->killed = uv_process_kill(&endpoint->process, SIGTERM) == 0;
            return JS::Value(endpoint->killed);
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "shutdown"_utf16,
        [owner](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            owner->shutdown();
            return JS::js_undefined();
        },
        0, JS::default_attributes);
    return *host;
}

JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_parent_host(
    JS::VM& vm, JS::Realm& realm, Mite::EventLoop& loop, int fd)
{
    if (uv_guess_handle(fd) != UV_NAMED_PIPE)
        return vm.throw_completion<JS::Error>("Missing inherited utility channel"sv);
    auto owner = service(vm, realm, loop, true);
    auto* endpoint = new Endpoint { *owner, 1, nullptr };
    owner->endpoints.emplace(1, endpoint);
    auto status = owner->initialize(*endpoint, 3);
    if (!status)
        status = uv_pipe_open(&endpoint->pipes[3].handle, fd);
    if (!status)
        status = owner->read(endpoint->pipes[3]);
    if (status) {
        owner->close(endpoint->pipes[3]);
        if (!endpoint->handles) {
            owner->endpoints.erase(1);
            delete endpoint;
        }

        return vm.throw_completion<JS::Error>("Cannot open inherited utility channel"sv);
    }

    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_native_function(
        realm, "setHandler"_utf16,
        [owner](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_function())
                return vm.throw_completion<JS::TypeError>("Utility handler must be callable"sv);
            auto found = owner->endpoints.find(1);
            if (owner->stopped || found == owner->endpoints.end())
                return vm.throw_completion<JS::Error>("Utility channel closed"sv);
            found->second->callback = vm.argument(0).as_function();
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "send"_utf16,
        [owner](JS::VM& vm) { return owner->send(vm, owner->get(JS::Value(1)), vm.argument(0)); },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "close"_utf16,
        [owner](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            if (auto* endpoint = owner->get(JS::Value(1)))
                owner->close_channel(*endpoint);
            return JS::js_undefined();
        },
        0, JS::default_attributes);
    return GC::Ref<JS::Object> { *host };
}
}
