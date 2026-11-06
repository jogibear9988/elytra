/**
 * Owns spawned processes, native pipes and pending exit/close notifications. Native callbacks
 * return through the runtime loop; argument/environment snapshots cannot retain mutable JS
 * storage.
 */

#include "platform/process/ChildProcess.h"
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
#include <string>
#include <unordered_map>
#include <uv.h>
#include <vector>

namespace Mite {
namespace {
    JS::Value text(JS::VM& vm, char const* value)
    {
        return JS::PrimitiveString::create(vm,
            Utf16String::from_utf8_with_replacement_character(StringView(value, strlen(value))));
    }

    JS::ThrowCompletionOr<std::string> string(JS::VM& vm, JS::Value value)
    {
        if (!value.is_string())
            return vm.throw_completion<JS::TypeError>("Invalid internal process string"sv);
        auto bytes = MUST(value.as_string().utf16_string_view().to_byte_string());
        if (bytes.contains('\0'))
            return vm.throw_completion<JS::TypeError>("NUL in internal process string"sv);
        return std::string(bytes.characters(), bytes.length());
    }

    JS::ThrowCompletionOr<std::vector<std::string>> strings(JS::VM& vm, JS::Value value)
    {
        if (!value.is_object() || !is<JS::Array>(value.as_object()))
            return vm.throw_completion<JS::TypeError>("Invalid internal process array"sv);
        auto& array = value.as_object();
        auto size = TRY(TRY(array.get("length"_utf16)).to_u32(vm));
        std::vector<std::string> result;
        for (uint32_t i = 0; i < size; ++i)
            result.push_back(TRY(string(vm, TRY(array.get(i)))));
        return result;
    }

    std::vector<char*> pointers(std::vector<std::string>& strings)
    {
        std::vector<char*> result;
        for (auto& item : strings)
            result.push_back(item.data());
        result.push_back(nullptr);
        return result;
    }

    JS::ThrowCompletionOr<int> integer(JS::VM& vm, JS::Value value, int minimum, int maximum)
    {
        if (!value.is_number() || !std::isfinite(value.as_double())
            || std::trunc(value.as_double()) != value.as_double() || value.as_double() < minimum
            || value.as_double() > maximum)
            return vm.throw_completion<JS::TypeError>("Invalid internal process integer"sv);
        return static_cast<int>(value.as_double());
    }

    struct Service;
    struct Child;

    struct Pipe {
        Child* child {};
        int index {};
        uv_pipe_t handle {};
        bool initialized {}, reading {}, eof {};

        uv_stream_t* stream()
        {
            return reinterpret_cast<uv_stream_t*>(&handle);
        }

        uv_handle_t* base()
        {
            return reinterpret_cast<uv_handle_t*>(&handle);
        }
    };

    struct Child {
        Service& owner;
        uint64_t id;
        GC::Root<JS::FunctionObject> callback;
        uv_process_t process {};
        std::array<Pipe, 3> pipes {};
        size_t handles {};
        bool process_initialized {}, started {}, exited {};

        uv_handle_t* base()
        {
            return reinterpret_cast<uv_handle_t*>(&process);
        }
    };

    struct Service {
        JS::VM& vm;
        JS::Realm& realm;
        EventLoop& loop;
        uint64_t next_id { 1 };
        std::unordered_map<uint64_t, Child*> children;

        ~Service()
        {
            VERIFY(children.empty());
        }

        Child* get(JS::Value value)
        {
            if (!value.is_number() || !std::isfinite(value.as_double())
                || std::trunc(value.as_double()) != value.as_double() || value.as_double() < 1
                || value.as_double() >= 9007199254740992.0)
                return nullptr;
            auto it = children.find(static_cast<uint64_t>(value.as_double()));
            return it == children.end() ? nullptr : it->second;
        }

        Pipe* pipe(JS::Value id, int index)
        {
            auto* child = get(id);
            if (!child)
                return nullptr;
            auto& pipe = child->pipes[static_cast<size_t>(index)];
            return pipe.initialized && !uv_is_closing(pipe.base()) ? &pipe : nullptr;
        }

        void emit(Child& child, char const* kind, int index = -1,
            JS::Value value = JS::js_undefined(), int signal = 0)
        {
            loop.dispatch([&] {
                return JS::call(vm, *child.callback, JS::js_undefined(), text(vm, kind),
                    JS::Value(index), value, JS::Value(signal));
            });
        }

        void released(Child& child)
        {
            VERIFY(child.handles);
            if (--child.handles == 0) {
                children.erase(child.id);
                delete &child;
            }
        }

        void close(Pipe& pipe)
        {
            if (!pipe.initialized || uv_is_closing(pipe.base()))
                return;
            uv_read_stop(pipe.stream());
            pipe.reading = false;
            uv_close(pipe.base(), [](uv_handle_t* handle) {
                auto& pipe = *static_cast<Pipe*>(handle->data);
                auto& child = *pipe.child;
                child.owner.emit(child, "pipeClose", pipe.index);
                child.owner.released(child);
            });
        }

        void close(Child& child)
        {
            if (!child.process_initialized || uv_is_closing(child.base()))
                return;
            uv_close(child.base(), [](uv_handle_t* handle) {
                auto& child = *static_cast<Child*>(handle->data);
                child.owner.emit(child, "processClose");
                child.owner.released(child);
            });
        }
    };

    struct Write {
        Pipe& pipe;
        uv_write_t request {};
        std::vector<char> bytes;
        GC::Root<JS::FunctionObject> callback;
    };

    struct Shutdown {
        Pipe& pipe;
        uv_shutdown_t request {};
        GC::Root<JS::FunctionObject> callback;
    };
}

GC::Ref<JS::Object> create_child_process_host(JS::VM& vm, JS::Realm& realm, EventLoop& loop)
{
    auto service = std::make_shared<Service>(vm, realm, loop);
    loop.on_shutdown([service] {
        // Node-style parent teardown closes owned monitoring/pipes, not the
        // independent OS process. Never call application JS during teardown.
        for (auto const& [id, child] : service->children) {
            for (auto& pipe : child->pipes)
                service->close(pipe);
            service->close(*child);
        }
    });
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_native_function(
        realm, "spawn"_utf16,
        [service, &realm](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto spec = TRY(vm.argument(0).to_object(vm));
            if (!vm.argument(1).is_function())
                return vm.throw_completion<JS::TypeError>("Invalid internal process callback"sv);
            auto file = TRY(string(vm, TRY(spec->get("file"_utf16))));
            auto args = TRY(strings(vm, TRY(spec->get("args"_utf16))));
            auto environment = TRY(strings(vm, TRY(spec->get("env"_utf16))));
            auto cwd = TRY(string(vm, TRY(spec->get("cwd"_utf16))));
            auto stdio = TRY(TRY(spec->get("stdio"_utf16)).to_object(vm));
            unsigned int flags = 0;
            if (TRY(spec->get("detached"_utf16)).to_boolean())
                flags |= UV_PROCESS_DETACHED;
            if (TRY(spec->get("windowsHide"_utf16)).to_boolean())
                flags |= UV_PROCESS_WINDOWS_HIDE;
            if (TRY(spec->get("windowsVerbatimArguments"_utf16)).to_boolean())
                flags |= UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS;
            std::array<int, 3> modes;
            for (int i = 0; i < 3; ++i)
                modes[static_cast<size_t>(i)]
                    = TRY(integer(vm, TRY(stdio->get(i)), -2, 0x7fffffff));
            if (args.empty() || file.empty())
                return vm.throw_completion<JS::TypeError>("Empty internal process command"sv);
            auto arguments = pointers(args), env = pointers(environment);
            VERIFY(service->next_id < 9007199254740992ULL);
            auto* child = new Child { *service, service->next_id++,
                GC::Root { vm.argument(1).as_function() } };
            service->children.emplace(child->id, child);
            std::array<uv_stdio_container_t, 3> handles {};
            int status = 0;
            for (int i = 0; i < 3; ++i) {
                auto mode = modes[static_cast<size_t>(i)];
                auto& entry = handles[static_cast<size_t>(i)];
                auto& pipe = child->pipes[static_cast<size_t>(i)];
                pipe.child = child;
                pipe.index = i;
                if (mode == -1) {
                    status = uv_pipe_init(&service->loop.native_loop(), &pipe.handle, 0);
                    if (status)
                        break;
                    pipe.initialized = true;
                    pipe.handle.data = &pipe;
                    ++child->handles;
                    entry.flags = static_cast<uv_stdio_flags>(
                        UV_CREATE_PIPE | (i == 0 ? UV_READABLE_PIPE : UV_WRITABLE_PIPE));
                    entry.data.stream = pipe.stream();
                } else if (mode == -2)
                    entry.flags = UV_IGNORE;
                else {
                    entry.flags = UV_INHERIT_FD;
                    entry.data.fd = mode;
                }
            }

            if (!status) {
                uv_process_options_t options {};
                options.file = file.c_str();
                options.args = arguments.data();
                options.env = env.data();
                options.cwd = cwd.empty() ? nullptr : cwd.c_str();
                options.stdio = handles.data();
                options.stdio_count = 3;
                options.flags = flags;
                options.exit_cb = [](uv_process_t* process, int64_t code, int signal) {
                    auto& child = *static_cast<Child*>(process->data);
                    child.exited = true;
                    child.owner.close(child.pipes[0]);
                    child.owner.emit(
                        child, "exit", -1, JS::Value(static_cast<double>(code)), signal);
                    child.owner.close(child);
                };

                status = uv_spawn(&service->loop.native_loop(), &child->process, &options);
                child->process.data = child;
                child->process_initialized = true;
                ++child->handles;
                child->started = !status;
            }

            GC::Root result { JS::Object::create(realm, nullptr) };
            result->define_direct_property(
                "id"_utf16, JS::Value(static_cast<double>(child->id)), JS::default_attributes);
            result->define_direct_property(
                "status"_utf16, JS::Value(status), JS::default_attributes);
            result->define_direct_property("pid"_utf16,
                status ? JS::js_undefined() : JS::Value(static_cast<double>(child->process.pid)),
                JS::default_attributes);
            int mask = child->process_initialized ? 8 : 0;
            for (size_t i = 0; i < 3; ++i)
                if (child->pipes[i].initialized)
                    mask |= 1 << i;
            result->define_direct_property(
                "handles"_utf16, JS::Value(mask), JS::default_attributes);
            if (status) {
                for (auto& pipe : child->pipes)
                    service->close(pipe);
                service->close(*child);
                if (!child->handles) {
                    service->children.erase(child->id);
                    delete child;
                }
            }

            return result.ptr();
        },
        2, JS::default_attributes);
    host->define_native_function(
        realm, "read"_utf16,
        [service](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto index = TRY(integer(vm, vm.argument(1), 1, 2));
            auto* pipe = service->pipe(vm.argument(0), index);
            if (!pipe)
                return JS::Value(UV_EBADF);
            if (!vm.argument(2).to_boolean()) {
                pipe->reading = false;
                return JS::Value(uv_read_stop(pipe->stream()));
            }

            if (pipe->reading || pipe->eof)
                return JS::Value(0);
            auto status = uv_read_start(
                pipe->stream(),
                [](uv_handle_t*, size_t size, uv_buf_t* buffer) {
                    size = std::min(size, size_t { 65536 });
                    buffer->base = static_cast<char*>(malloc(size));
                    buffer->len = buffer->base ? size : 0;
                },
                [](uv_stream_t* stream, ssize_t count, uv_buf_t const* buffer) {
                    auto& pipe = *static_cast<Pipe*>(stream->data);
                    auto& child = *pipe.child;
                    auto& owner = child.owner;
                    if (count > 0)
                        owner.loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                            GC::Root bytes { TRY(
                                JS::Uint8Array::create(owner.realm, static_cast<size_t>(count))) };
                            bytes->viewed_array_buffer()->overwrite(
                                0, buffer->base, static_cast<size_t>(count));
                            return JS::call(owner.vm, *child.callback, JS::js_undefined(),
                                text(owner.vm, "data"), JS::Value(pipe.index),
                                JS::Value(bytes.ptr()), JS::Value(0));
                        });
                    else if (count < 0) {
                        uv_read_stop(stream);
                        pipe.reading = false;
                        pipe.eof = true;
                        owner.emit(child, count == UV_EOF ? "end" : "pipeError", pipe.index,
                            JS::Value(static_cast<int>(count)));
                    }

                    free(buffer->base);
                });
            if (!status)
                pipe->reading = true;
            return JS::Value(status);
        },
        3, JS::default_attributes);
    host->define_native_function(
        realm, "write"_utf16,
        [service](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* pipe = service->pipe(vm.argument(0), 0);
            if (!pipe)
                return JS::Value(UV_EPIPE);
            auto data = vm.argument(1);
            if (!data.is_object() || !is<JS::Uint8Array>(data.as_object())
                || !vm.argument(2).is_function())
                return vm.throw_completion<JS::TypeError>("Invalid internal process write"sv);
            auto& array = static_cast<JS::Uint8Array&>(data.as_object());
            auto witness = JS::make_typed_array_with_buffer_witness_record(
                array, JS::ArrayBuffer::Order::SeqCst);
            if (JS::is_typed_array_out_of_bounds(witness))
                return vm.throw_completion<JS::TypeError>("Detached process write"sv);
            auto size = JS::typed_array_byte_length(witness);
            if (size > 0x7fffffff)
                return vm.throw_completion<JS::RangeError>(
                    "Process pipe write exceeds byte limit"sv);
            auto* write = new Write { *pipe, {}, {}, GC::Root { vm.argument(2).as_function() } };
            if (size) {
                auto* bytes = reinterpret_cast<char const*>(
                    array.viewed_array_buffer()->data_at(array.byte_offset()));
                write->bytes.assign(bytes, bytes + size);
            }

            write->request.data = write;
            auto buffer = uv_buf_init(write->bytes.data(), static_cast<unsigned int>(size));
            int status = uv_write(
                &write->request, pipe->stream(), &buffer, 1, [](uv_write_t* request, int status) {
                    auto* write = static_cast<Write*>(request->data);
                    auto& owner = write->pipe.child->owner;
                    owner.loop.dispatch([&] {
                        return JS::call(
                            owner.vm, *write->callback, JS::js_undefined(), JS::Value(status));
                    });
                    delete write;
                });
            if (status)
                delete write;
            return JS::Value(status);
        },
        3, JS::default_attributes);
    host->define_native_function(
        realm, "shutdown"_utf16,
        [service](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* pipe = service->pipe(vm.argument(0), 0);
            if (!pipe)
                return JS::Value(UV_EPIPE);
            if (!vm.argument(1).is_function())
                return vm.throw_completion<JS::TypeError>("Invalid internal process shutdown"sv);
            auto* shutdown = new Shutdown { *pipe, {}, GC::Root { vm.argument(1).as_function() } };
            shutdown->request.data = shutdown;
            int status = uv_shutdown(
                &shutdown->request, pipe->stream(), [](uv_shutdown_t* request, int status) {
                    auto* shutdown = static_cast<Shutdown*>(request->data);
                    auto& owner = shutdown->pipe.child->owner;
                    owner.loop.dispatch([&] {
                        return JS::call(
                            owner.vm, *shutdown->callback, JS::js_undefined(), JS::Value(status));
                    });
                    delete shutdown;
                });
            if (status)
                delete shutdown;
            return JS::Value(status);
        },
        2, JS::default_attributes);
    host->define_native_function(
        realm, "closePipe"_utf16,
        [service](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto index = TRY(integer(vm, vm.argument(1), 0, 2));
            if (auto* pipe = service->pipe(vm.argument(0), index))
                service->close(*pipe);
            return JS::js_undefined();
        },
        2, JS::default_attributes);
    host->define_native_function(
        realm, "ref"_utf16,
        [service](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto index = TRY(integer(vm, vm.argument(1), -1, 2));
            auto* child = service->get(vm.argument(0));
            if (!child)
                return JS::js_undefined();
            uv_handle_t* handle
                = index == -1 && child->process_initialized && !uv_is_closing(child->base())
                ? child->base()
                : nullptr;
            if (index >= 0)
                if (auto* pipe = service->pipe(vm.argument(0), index))
                    handle = pipe->base();
            if (handle) {
                if (vm.argument(2).to_boolean())
                    uv_ref(handle);
                else
                    uv_unref(handle);
            }

            return JS::js_undefined();
        },
        3, JS::default_attributes);
    host->define_native_function(
        realm, "kill"_utf16,
        [service](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto signal = TRY(integer(vm, vm.argument(1), 0, 0x7fffffff));
            auto* child = service->get(vm.argument(0));
            if (!child || !child->started || child->exited || uv_is_closing(child->base()))
                return JS::Value(UV_ESRCH);
            return JS::Value(uv_process_kill(&child->process, signal));
        },
        2, JS::default_attributes);
    host->define_native_function(
        realm, "error"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto status = TRY(integer(vm, vm.argument(0), -65536, -1));
            GC::Root result { JS::Object::create(vm.realm(), nullptr) };
            result->define_direct_property(
                "code"_utf16, text(vm, uv_err_name(status)), JS::default_attributes);
            result->define_direct_property(
                "message"_utf16, text(vm, uv_strerror(status)), JS::default_attributes);
            return result.ptr();
        },
        1, JS::default_attributes);
    return *host;
}
}
