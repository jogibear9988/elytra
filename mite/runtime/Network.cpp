#include "Network.h"
#include "EventLoop.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Mite {
namespace {
static Utf16String text(char const* value) { return Utf16String::from_utf8(StringView(value, strlen(value))); }
static JS::Value js_text(JS::VM& vm, char const* value) { return JS::PrimitiveString::create(vm, text(value)); }
static std::string string(JS::Value value) {
    auto bytes = MUST(value.as_string().utf16_string_view().to_byte_string());
    return { bytes.characters(), bytes.length() };
}
static int address(std::string const& name, int port, sockaddr_storage& storage) {
    if (name.find('\0') != std::string::npos) return UV_EINVAL;
    int result = uv_ip4_addr(name.c_str(), port, reinterpret_cast<sockaddr_in*>(&storage));
    if (result) result = uv_ip6_addr(name.c_str(), port, reinterpret_cast<sockaddr_in6*>(&storage));
    return result;
}
struct Transport;
struct Tcp {
    Transport& owner;
    uint64_t id;
    uv_tcp_t handle {};
    GC::Root<JS::FunctionObject> callback;
    bool notify_close { true };
    bool reading { false };
    uv_stream_t* stream() { return reinterpret_cast<uv_stream_t*>(&handle); }
    uv_handle_t* base() { return reinterpret_cast<uv_handle_t*>(&handle); }
};
struct Transport {
    JS::VM& vm;
    JS::Realm& realm;
    EventLoop& loop;
    uint64_t next_id { 1 };
    std::unordered_map<uint64_t, Tcp*> sockets;
    ~Transport() { VERIFY(sockets.empty()); }
    Tcp* get(JS::Value id) {
        if (!id.is_number() || !std::isfinite(id.as_double()) || id.as_double() < 1
            || id.as_double() >= 9007199254740992.0 || std::trunc(id.as_double()) != id.as_double()) return nullptr;
        auto it = sockets.find(static_cast<uint64_t>(id.as_double()));
        if (it == sockets.end() || uv_is_closing(it->second->base())) return nullptr;
        return it->second;
    }
    Tcp* create(JS::FunctionObject& callback) {
        VERIFY(next_id < 9007199254740992ULL);
        auto* socket = new Tcp { *this, next_id++, {}, GC::Root { callback } };
        if (uv_tcp_init(&loop.native_loop(), &socket->handle)) { delete socket; return nullptr; }
        socket->handle.data = socket;
        sockets.emplace(socket->id, socket);
        return socket;
    }
    void emit(Tcp& socket, char const* type, JS::Value value = JS::js_undefined()) {
        loop.dispatch([&]() { return JS::call(vm, *socket.callback, JS::js_undefined(), js_text(vm, type), value); });
    }
    void close(Tcp& socket) {
        if (uv_is_closing(socket.base())) return;
        uv_read_stop(socket.stream());
        socket.reading = false;
        uv_close(socket.base(), [](uv_handle_t* handle) {
            auto* socket = static_cast<Tcp*>(handle->data);
            auto& owner = socket->owner;
            owner.sockets.erase(socket->id);
            if (socket->notify_close) owner.emit(*socket, "close");
            delete socket;
        });
    }
};
struct Write {
    Tcp& socket;
    uv_write_t request {};
    std::vector<char> bytes;
    GC::Root<JS::FunctionObject> callback;
};
struct Shutdown {
    Tcp& socket;
    uv_shutdown_t request {};
    GC::Root<JS::FunctionObject> callback;
};
static JS::ThrowCompletionOr<int> number(JS::VM& vm, JS::Value value, int minimum, int maximum) {
    if (!value.is_number() || !std::isfinite(value.as_double()) || std::trunc(value.as_double()) != value.as_double()
        || value.as_double() < minimum || value.as_double() > maximum)
        return vm.throw_completion<JS::TypeError>("Invalid internal network number"_utf16);
    return static_cast<int>(value.as_double());
}
static JS::ThrowCompletionOr<void> callback(JS::VM& vm, JS::Value value) {
    if (!value.is_function()) return vm.throw_completion<JS::TypeError>("Invalid internal network callback"_utf16);
    return {};
}
}

int network_ip_family(StringView value)
{
    sockaddr_storage storage {};
    if (address(std::string(value.characters_without_null_termination(), value.length()), 0, storage)) return 0;
    return storage.ss_family == AF_INET ? 4 : 6;
}

GC::Ref<JS::Object> create_network_host(JS::VM& vm, JS::Realm& realm, EventLoop& loop)
{
    auto transport = std::make_shared<Transport>(vm, realm, loop);
    loop.on_shutdown([transport] { for (auto const& [id, socket] : transport->sockets) transport->close(*socket); });
    auto host = JS::Object::create(realm, nullptr);
    host->define_native_function(realm, "create"_utf16, [transport](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        TRY(callback(vm, vm.argument(0)));
        auto* socket = transport->create(vm.argument(0).as_function());
        if (!socket) return vm.throw_completion<JS::Error>("Could not initialize TCP handle"_utf16);
        return JS::Value(static_cast<double>(socket->id));
    }, 1, JS::default_attributes);
    host->define_native_function(realm, "setCallback"_utf16, [transport](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        TRY(callback(vm, vm.argument(1)));
        if (auto* socket = transport->get(vm.argument(0))) { socket->callback = vm.argument(1).as_function(); socket->notify_close = true; }
        return JS::js_undefined();
    }, 2, JS::default_attributes);
    host->define_native_function(realm, "isIP"_utf16, +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        if (!vm.argument(0).is_string()) return JS::Value(0);
        auto name = string(vm.argument(0));
        return JS::Value(network_ip_family(StringView(name.data(), name.size())));
    }, 1, JS::default_attributes);
    host->define_native_function(realm, "error"_utf16, +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        int status = TRY(number(vm, vm.argument(0), -65536, -1));
        auto result = JS::Object::create(vm.realm(), nullptr);
        result->define_direct_property("code"_utf16, js_text(vm, uv_err_name(status)), JS::default_attributes);
        result->define_direct_property("message"_utf16, js_text(vm, uv_strerror(status)), JS::default_attributes);
        return result;
    }, 1, JS::default_attributes);
    host->define_native_function(realm, "listen"_utf16, [transport](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto* socket = transport->get(vm.argument(0));
        if (!socket) return JS::Value(UV_EBADF);
        if (!vm.argument(1).is_string()) return vm.throw_completion<JS::TypeError>("Invalid internal network address"_utf16);
        int port = TRY(number(vm, vm.argument(2), 0, 65535));
        int backlog = TRY(number(vm, vm.argument(3), 1, 2147483647));
        sockaddr_storage storage {};
        int status = address(string(vm.argument(1)), port, storage);
        if (!status) status = uv_tcp_bind(&socket->handle, reinterpret_cast<sockaddr*>(&storage), vm.argument(4).to_boolean() ? UV_TCP_IPV6ONLY : 0);
        if (!status) status = uv_listen(socket->stream(), backlog, [](uv_stream_t* handle, int status) {
            auto& server = *static_cast<Tcp*>(handle->data);
            auto& owner = server.owner;
            if (status) { owner.emit(server, "error", JS::Value(status)); return; }
            auto* client = owner.create(*server.callback);
            if (!client) { owner.emit(server, "error", JS::Value(UV_ENOMEM)); return; }
            client->notify_close = false;
            status = uv_accept(handle, client->stream());
            if (status) { owner.close(*client); owner.emit(server, "error", JS::Value(status)); return; }
            owner.emit(server, "connection", JS::Value(static_cast<double>(client->id)));
        });
        return JS::Value(status);
    }, 5, JS::default_attributes);
    host->define_native_function(realm, "connect"_utf16, [transport](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto* socket = transport->get(vm.argument(0));
        if (!socket) return JS::Value(UV_EBADF);
        if (!vm.argument(1).is_string()) return vm.throw_completion<JS::TypeError>("Invalid internal network address"_utf16);
        int port = TRY(number(vm, vm.argument(2), 0, 65535));
        sockaddr_storage storage {};
        int status = address(string(vm.argument(1)), port, storage);
        if (status) return JS::Value(status);
        auto* request = new uv_connect_t {};
        request->data = socket;
        status = uv_tcp_connect(request, &socket->handle, reinterpret_cast<sockaddr*>(&storage), [](uv_connect_t* request, int status) {
            auto& socket = *static_cast<Tcp*>(request->data);
            socket.owner.emit(socket, "connect", JS::Value(status));
            delete request;
        });
        if (status) delete request;
        return JS::Value(status);
    }, 3, JS::default_attributes);
    host->define_native_function(realm, "read"_utf16, [transport](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto* socket = transport->get(vm.argument(0));
        if (!socket) return JS::Value(UV_EBADF);
        if (!vm.argument(1).to_boolean()) { socket->reading = false; return JS::Value(uv_read_stop(socket->stream())); }
        if (socket->reading) return JS::Value(0);
        int status = uv_read_start(socket->stream(), [](uv_handle_t*, size_t size, uv_buf_t* buffer) {
            // Each read callback owns and frees its allocation, even on EOF.
            size = std::min(size, size_t { 65536 });
            buffer->base = static_cast<char*>(malloc(size));
            buffer->len = buffer->base ? size : 0;
        }, [](uv_stream_t* handle, ssize_t count, uv_buf_t const* buffer) {
            auto& socket = *static_cast<Tcp*>(handle->data);
            auto& owner = socket.owner;
            if (count > 0) {
                owner.loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                    auto bytes = TRY(JS::Uint8Array::create(owner.realm, count));
                    bytes->viewed_array_buffer()->overwrite(0, buffer->base, count);
                    return JS::call(owner.vm, *socket.callback, JS::js_undefined(), js_text(owner.vm, "data"), JS::Value(bytes));
                });
            } else if (count < 0) {
                uv_read_stop(handle);
                socket.reading = false;
                owner.emit(socket, count == UV_EOF ? "end" : "error", JS::Value(static_cast<int>(count)));
            }
            free(buffer->base);
        });
        if (!status) socket->reading = true;
        return JS::Value(status);
    }, 2, JS::default_attributes);
    host->define_native_function(realm, "write"_utf16, [transport](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto* socket = transport->get(vm.argument(0));
        if (!socket) return JS::Value(UV_EBADF);
        TRY(callback(vm, vm.argument(2)));
        auto value = vm.argument(1);
        if (!value.is_object() || !is<JS::Uint8Array>(value.as_object())) return vm.throw_completion<JS::TypeError>("Invalid internal network bytes"_utf16);
        auto& array = static_cast<JS::Uint8Array&>(value.as_object());
        auto witness = JS::make_typed_array_with_buffer_witness_record(array, JS::ArrayBuffer::Order::SeqCst);
        if (JS::is_typed_array_out_of_bounds(witness)) return vm.throw_completion<JS::TypeError>("Detached network bytes"_utf16);
        size_t size = JS::typed_array_byte_length(witness);
        if (size > std::numeric_limits<unsigned int>::max()) return vm.throw_completion<JS::RangeError>("Network write exceeds native buffer limit"_utf16);
        auto* request = new Write { *socket, {}, {}, GC::Root { vm.argument(2).as_function() } };
        if (size) {
            auto* data = reinterpret_cast<char const*>(array.viewed_array_buffer()->data_at(array.byte_offset()));
            request->bytes.assign(data, data + size);
        }
        request->request.data = request;
        auto buffer = uv_buf_init(request->bytes.data(), static_cast<unsigned int>(size));
        // Match a native stream's immediate completion path. In particular an
        // empty write must not add an IOCP completion after already-sent HTTP
        // headers. Return 1 for synchronous success; JS Writable defers public
        // callbacks itself. Never call JS reentrantly from this native frame.
        int written = 0;
        if (size)
            written = uv_try_write(socket->stream(), &buffer, 1);
        else if (uv_stream_get_write_queue_size(socket->stream()))
            written = UV_EAGAIN;
        if (written >= 0 && static_cast<size_t>(written) == size) {
            delete request;
            return JS::Value(1);
        }
        if (written < 0 && written != UV_EAGAIN && written != UV_ENOSYS) {
            delete request;
            return JS::Value(written);
        }
        if (written > 0) {
            buffer.base += written;
            buffer.len -= written;
        }
        // The owned allocation retains the entire original payload until the
        // remaining suffix completes, including partial immediate writes.
        int status = uv_write(&request->request, socket->stream(), &buffer, 1, [](uv_write_t* native, int status) {
            auto* request = static_cast<Write*>(native->data);
            auto& owner = request->socket.owner;
            owner.loop.dispatch([&] { return JS::call(owner.vm, *request->callback, JS::js_undefined(), JS::Value(status)); });
            delete request;
        });
        if (status) delete request;
        return JS::Value(status);
    }, 3, JS::default_attributes);
    host->define_native_function(realm, "shutdown"_utf16, [transport](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto* socket = transport->get(vm.argument(0));
        if (!socket) return JS::Value(UV_EBADF);
        TRY(callback(vm, vm.argument(1)));
        auto* request = new Shutdown { *socket, {}, GC::Root { vm.argument(1).as_function() } };
        request->request.data = request;
        int status = uv_shutdown(&request->request, socket->stream(), [](uv_shutdown_t* native, int status) {
            auto* request = static_cast<Shutdown*>(native->data);
            auto& owner = request->socket.owner;
            owner.loop.dispatch([&] { return JS::call(owner.vm, *request->callback, JS::js_undefined(), JS::Value(status)); });
            delete request;
        });
        if (status) delete request;
        return JS::Value(status);
    }, 2, JS::default_attributes);
    host->define_native_function(realm, "close"_utf16, [transport](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        if (auto* socket = transport->get(vm.argument(0))) transport->close(*socket);
        return JS::js_undefined();
    }, 1, JS::default_attributes);
    host->define_native_function(realm, "ref"_utf16, [transport](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        if (auto* socket = transport->get(vm.argument(0))) {
            if (vm.argument(1).to_boolean()) uv_ref(socket->base()); else uv_unref(socket->base());
        }
        return JS::js_undefined();
    }, 2, JS::default_attributes);
    host->define_native_function(realm, "option"_utf16, [transport](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto* socket = transport->get(vm.argument(0));
        if (!socket) return JS::Value(UV_EBADF);
        int option = TRY(number(vm, vm.argument(1), 0, 1));
        int delay = TRY(number(vm, vm.argument(3), 0, 2147483647));
        return JS::Value(option == 0 ? uv_tcp_nodelay(&socket->handle, vm.argument(2).to_boolean())
                                    : uv_tcp_keepalive(&socket->handle, vm.argument(2).to_boolean(), delay));
    }, 4, JS::default_attributes);
    host->define_native_function(realm, "address"_utf16, [transport](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto* socket = transport->get(vm.argument(0));
        if (!socket) return JS::js_null();
        sockaddr_storage storage {};
        int size = sizeof(storage);
        int status = vm.argument(1).to_boolean() ? uv_tcp_getpeername(&socket->handle, reinterpret_cast<sockaddr*>(&storage), &size)
                                               : uv_tcp_getsockname(&socket->handle, reinterpret_cast<sockaddr*>(&storage), &size);
        if (status) return JS::js_null();
        char name[INET6_ADDRSTRLEN] {};
        int port;
        if (storage.ss_family == AF_INET) {
            auto& addr = reinterpret_cast<sockaddr_in&>(storage);
            uv_ip4_name(&addr, name, sizeof(name)); port = ntohs(addr.sin_port);
        } else {
            auto& addr = reinterpret_cast<sockaddr_in6&>(storage);
            uv_ip6_name(&addr, name, sizeof(name)); port = ntohs(addr.sin6_port);
        }
        auto result = JS::Object::create(vm.realm(), vm.realm().intrinsics().object_prototype());
        result->define_direct_property("address"_utf16, js_text(vm, name), JS::default_attributes);
        result->define_direct_property("family"_utf16, js_text(vm, storage.ss_family == AF_INET ? "IPv4" : "IPv6"), JS::default_attributes);
        result->define_direct_property("port"_utf16, JS::Value(port), JS::default_attributes);
        return result;
    }, 2, JS::default_attributes);
    return host;
}
}
