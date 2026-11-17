/**
 * Files (all OS) and POSIX terminals write synchronously. POSIX pipes/sockets
 * first try the kernel directly, then retain only the unwritten suffix in a
 * libuv write request. No stdio write uses the filesystem worker pool.
 * Original descriptors are borrowed: close only owned duplicates, never call
 * shutdown(), and restore shared nonblocking state after the last owner closes.
 * Embedders keep fd1/2 stable while runtimes overlap; the first lease anchors
 * their output identities. External descriptor replacement is not retargeted.
 */
#include "platform/stdio/Host.h"
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include "core/NativeByteSource.h"
#include "platform/UV.h"
#include "platform/process/InheritedPipe.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_set>
#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#else
#include <io.h>
#endif

namespace Mite {
namespace {
    constexpr size_t chunk_limit = 2 * 1024 * 1024;
    constexpr size_t queue_limit = 8 * 1024 * 1024;

    int duplicate(int fd)
    {
#ifndef _WIN32
        return fcntl(fd, F_DUPFD_CLOEXEC, 3);
#else
        ScopedDescriptorProbe guard;
        std::array<int, 3> low {};
        size_t count = 0;
        auto result = _dup(fd);
        // Own only descriptors above stderr, even when the host closed stdin.
        while (result >= 0 && result <= 2) {
            low[count++] = result;
            result = _dup(fd);
        }

        auto saved = errno;
        while (count)
            _close(low[--count]);
        errno = saved;
        if (result >= 0) {
            auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(result));
            if (!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0)) {
                _close(result);
                return -1;
            }
        }

        return result;
#endif
    }

    void close_descriptor(int fd)
    {
#ifndef _WIN32
        close(fd);
#else
        _close(fd);
#endif
    }

    struct Anchor {
        int descriptor { -1 }, error { UV_EBADF };
        uv_handle_type type { UV_UNKNOWN_HANDLE };
#ifndef _WIN32
        int flags { -1 };
#endif
    };

    struct LeaseGroup {
        std::array<Anchor, 2> outputs;

        LeaseGroup()
        {
            // Snapshot BOTH descriptors before any uv handle can set O_NONBLOCK.
            // fd1/fd2 can alias one open-file-description; per-fd leases are unsafe.
            for (size_t index = 0; index < outputs.size(); ++index) {
                auto& output = outputs[index];
                output.descriptor = duplicate(static_cast<int>(index) + 1);
                if (output.descriptor < 0) {
                    output.error = uv_translate_sys_error(errno);
                    continue;
                }

                output.type = uv_guess_handle(output.descriptor);
                output.error = 0;
#ifndef _WIN32
                output.flags = fcntl(output.descriptor, F_GETFL);
                if (output.flags < 0)
                    output.error = uv_translate_sys_error(errno);
#endif
            }
        }

        void restore_flags()
        {
            for (auto& output : outputs) {
                if (output.descriptor < 0)
                    continue;
#ifndef _WIN32
                auto current = fcntl(output.descriptor, F_GETFL);
                if (current >= 0 && output.flags >= 0) {
                    // Restore only our shared O_NONBLOCK bit, preserving unrelated
                    // status flags. Anchors survive host numeric-fd replacement.
                    auto restored = (current & ~O_NONBLOCK) | (output.flags & O_NONBLOCK);
                    if (restored != current)
                        (void)fcntl(output.descriptor, F_SETFL, restored);
                }
#endif
            }
        }

        void restore()
        {
            restore_flags();
            for (auto& output : outputs) {
                if (output.descriptor >= 0)
                    close_descriptor(output.descriptor);
                output.descriptor = -1;
            }
        }
    };

    std::mutex lease_mutex;
    std::shared_ptr<LeaseGroup> active_group;
    size_t lease_users {};
    bool exit_hook_attempted {}, exit_hook_registered {};

    std::shared_ptr<LeaseGroup> acquire_group()
    {
        std::lock_guard lock(lease_mutex);
        if (!active_group)
            active_group = std::make_shared<LeaseGroup>();
        if (!exit_hook_attempted) {
            exit_hook_attempted = true;
            exit_hook_registered = std::atexit(restore_stdio_before_process_exit) == 0;
        }

        ++lease_users;
        return active_group;
    }

    void release_group(std::shared_ptr<LeaseGroup>& group)
    {
        std::lock_guard lock(lease_mutex);
        VERIFY(group && active_group == group && lease_users);
        if (--lease_users == 0) {
            active_group->restore();
            active_group.reset();
        }

        group.reset();
    }

    JS::Value error(JS::VM& vm, int status)
    {
        auto message = uv_strerror(status);
        GC::Root result { JS::Error::create(vm.realm(),
            Utf16String::from_utf8_with_replacement_character(
                StringView(message, strlen(message)))) };
        auto code = uv_err_name(status);
        result->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(StringView(code, strlen(code)))),
            JS::default_attributes);
        result->define_direct_property("errno"_utf16, JS::Value(status), JS::default_attributes);
        result->define_direct_property("syscall"_utf16,
            JS::PrimitiveString::create(vm, "write"_utf16), JS::default_attributes);
        return result;
    }

    struct Service;

    struct Output {
        Service* owner {};
        uv_pipe_t pipe {};
        uv_tcp_t tcp {};
        uv_tty_t tty {};
        uv_stream_t* stream {};
        bool initialized {}, closed {};
        int initialization_error {};
        size_t pending {};
    };

    struct Write {
        Service& owner;
        Output& output;
        uv_write_t request {};
        std::vector<char> bytes;
        GC::Root<JS::FunctionObject> callback;
        GC::Root<AsyncCarrier> context;
    };

    struct Service {
        JS::Realm& realm;
        EventLoop& loop;
        std::shared_ptr<LeaseGroup> lease { acquire_group() };
        std::array<Output, 2> outputs {};
        std::unordered_set<Write*> writes;
        size_t queued {};
        bool stopping {};

        Service(JS::Realm& realm, EventLoop& loop)
            : realm(realm)
            , loop(loop)
        {
            for (auto& output : outputs)
                output.owner = this;
        }

        void visit_capture_edges(GC::Cell::Visitor& visitor) const
        {
            if (stopping)
                return;
            visitor.visit(realm);
            for (auto* write : writes) {
                visitor.visit(write->callback.ptr());
                visitor.visit(write->context.ptr());
            }
        }

        void release_if_closed()
        {
            if (!stopping || !lease || !writes.empty())
                return;
            for (auto const& output : outputs)
                if (output.initialized && !output.closed)
                    return;
            release_group(lease);
        }

        void stop()
        {
            stopping = true;
            for (auto* write : writes) {
                write->callback = nullptr;
                write->context = nullptr;
            }

            for (auto& output : outputs) {
                if (!output.initialized || output.closed)
                    continue;
                auto* handle = reinterpret_cast<uv_handle_t*>(output.stream);
                if (uv_is_closing(handle))
                    continue;
                // uv_close cancels pending output without shutdown() on the
                // borrowed pipe/socket's shared open-file-description.
                uv_close(handle, [](uv_handle_t* handle) {
                    auto& output = *static_cast<Output*>(handle->data);
                    output.closed = true;
                    output.owner->release_if_closed();
                });
            }

            release_if_closed();
        }

        bool synchronous(Anchor const& anchor)
        {
#ifdef _WIN32
            return anchor.type == UV_FILE || anchor.type == UV_NAMED_PIPE;
#else
            return anchor.type == UV_FILE || anchor.type == UV_TTY;
#endif
        }

        int initialize(Output& output, Anchor const& anchor)
        {
            if (output.initialization_error)
                return output.initialization_error;
            if (output.initialized)
                return output.closed ? UV_EPIPE : 0;
            if (!exit_hook_registered)
                return UV_ENOSYS;
#ifdef _WIN32
            // CRT socket descriptors are not Winsock SOCKET values. Only the
            // POSIX provider adopts TCP stdio; do not miscast one on Windows.
            if (anchor.type == UV_TCP)
                return UV_ENOSYS;
#endif
            auto descriptor = duplicate(anchor.descriptor);
            if (descriptor < 0)
                return uv_translate_sys_error(errno);
            int status = 0;
            if (anchor.type == UV_NAMED_PIPE) {
                status = uv_pipe_init(&loop.native_loop(), &output.pipe, 0);
                output.stream = reinterpret_cast<uv_stream_t*>(&output.pipe);
                if (!status) {
                    output.initialized = true;
                    status = uv_pipe_open(&output.pipe, descriptor);
                }
            } else if (anchor.type == UV_TCP) {
                status = uv_tcp_init(&loop.native_loop(), &output.tcp);
                output.stream = reinterpret_cast<uv_stream_t*>(&output.tcp);
                if (!status) {
                    output.initialized = true;
                    status = uv_tcp_open(&output.tcp, descriptor);
                }
#ifdef _WIN32
            } else if (anchor.type == UV_TTY) {
                status = uv_tty_init(&loop.native_loop(), &output.tty, descriptor, 0);
                output.stream = reinterpret_cast<uv_stream_t*>(&output.tty);
                output.initialized = !status;
#endif
            } else {
                status = UV_ENOSYS;
            }

            if (status) {
                close_descriptor(descriptor);
                output.initialization_error = status;
            }

            if (output.initialized) {
                output.stream->data = &output;
                uv_unref(reinterpret_cast<uv_handle_t*>(output.stream));
            }

            return status;
        }

        JS::ThrowCompletionOr<JS::Value> write(JS::VM& vm)
        {
            if (!vm.argument(0).is_int32()
                || (vm.argument(0).as_i32() != 1 && vm.argument(0).as_i32() != 2)
                || !vm.argument(2).is_function())
                return byte_source_error(
                    vm, "ERR_INVALID_ARG_TYPE", "Invalid native stdio write"_utf16);
            auto source = TRY(native_byte_source(vm, vm.argument(1), false));
            GC::Root callback { vm.argument(2).as_function() };
            auto complete = [&](int status) {
                return JS::call(vm, *callback, JS::js_undefined(),
                    status ? error(vm, status) : JS::js_undefined());
            };

            if (stopping)
                return complete(UV_EPIPE);
            auto index = static_cast<size_t>(vm.argument(0).as_i32() - 1);
            auto& anchor = lease->outputs[index];
            auto& output = outputs[index];
            if (anchor.error)
                return complete(anchor.error);
            if (source.length > chunk_limit || source.length > queue_limit - queued)
                return complete(UV_ENOBUFS);
            auto const* data = reinterpret_cast<char const*>(source.buffer->data_at(source.offset));
            if (!source.length)
                return complete(0);
            if (synchronous(anchor)) {
                size_t offset = 0;
                while (offset < source.length) {
                    auto buffer = uv_buf_init(const_cast<char*>(data + offset),
                        static_cast<unsigned int>(source.length - offset));
                    uv_fs_t request {};
                    auto count = uv_fs_write(
                        nullptr, &request, anchor.descriptor, &buffer, 1, -1, nullptr);
                    uv_fs_req_cleanup(&request);
#ifndef _WIN32
                    if (count == UV_EAGAIN) {
                        pollfd descriptor { anchor.descriptor, POLLOUT, 0 };
                        int ready;
                        do {
                            ready = poll(&descriptor, 1, -1);
                        } while (ready < 0 && errno == EINTR);
                        if (ready >= 0)
                            continue;
                        return complete(uv_translate_sys_error(errno));
                    }
#endif
                    if (count < 0)
                        return complete(static_cast<int>(count));
                    if (!count)
                        return complete(UV_EIO);
                    offset += static_cast<size_t>(count);
                }

                return complete(0);
            }

            auto status = initialize(output, anchor);
            if (status)
                return complete(status);
            size_t offset = 0;
            if (!output.pending) {
                auto buffer = uv_buf_init(
                    const_cast<char*>(data), static_cast<unsigned int>(source.length));
                auto count = uv_try_write(output.stream, &buffer, 1);
                if (count > 0)
                    offset = static_cast<size_t>(count);
                else if (count != UV_EAGAIN && count != UV_ENOSYS && count < 0)
                    return complete(static_cast<int>(count));
            }

            if (offset == source.length)
                return complete(0);
            auto* request = new Write { *this, output, {},
                std::vector<char>(data + offset, data + source.length), GC::Root { *callback },
                GC::Root { loop.async_context().capture() } };
            request->request.data = request;
            queued += request->bytes.size();
            ++output.pending;
            writes.insert(request);
            auto buffer = uv_buf_init(
                request->bytes.data(), static_cast<unsigned int>(request->bytes.size()));
            status = uv_write(
                &request->request, output.stream, &buffer, 1, [](uv_write_t* handle, int status) {
                    auto* request = static_cast<Write*>(handle->data);
                    auto& service = request->owner;
                    GC::Root<JS::FunctionObject> callback { request->callback.ptr() };
                    GC::Root<AsyncCarrier> carrier { request->context.ptr() };
                    service.writes.erase(request);
                    service.queued -= request->bytes.size();
                    --request->output.pending;
                    // Release the owned suffix BEFORE author callbacks can
                    // re-enter write(), keeping the actual byte budget honest.
                    delete request;
                    if (!service.stopping && callback) {
                        AsyncContext::Scope context(service.loop.async_context(), carrier.ptr());
                        service.loop.dispatch([&] {
                            return JS::call(service.realm.vm(), *callback, JS::js_undefined(),
                                status ? error(service.realm.vm(), status) : JS::js_undefined());
                        });
                    }

                    service.release_if_closed();
                });
            if (status) {
                writes.erase(request);
                queued -= request->bytes.size();
                --output.pending;
                delete request;
                return complete(status);
            }

            return JS::js_undefined();
        }
    };
}

void restore_stdio_before_process_exit()
{
    std::lock_guard lock(lease_mutex);
    // Other owner threads may still hold a lease on process-ending paths. Do
    // not close anchors/reset state or promise to flush queued application data.
    if (active_group)
        active_group->restore_flags();
}

GC::Ref<JS::Object> create_stdio_host(JS::Realm& realm, EventLoop& loop)
{
    auto service = std::make_shared<Service>(realm, loop);
    loop.on_shutdown([service] { service->stop(); });
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_native_function(realm, "write"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(service)),
            [](auto& service, JS::VM& vm) { return service->write(vm); }),
        3, JS::default_attributes);
    return *host;
}
}
