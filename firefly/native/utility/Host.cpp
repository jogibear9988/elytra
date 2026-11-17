/** Owned libuv subprocesses and bounded binary frames, independent of the renderer transport.
 * Every OS callback executes on Mite's owner thread and enters JS through dispatch().
 * The parent kills and reaps owned children at teardown; no JS runs after shutdown.
 */
#include "Host.h"
#include "../platform/GCIncludes.h"
#include "core/EventLoop.h"
#include "platform/UV.h"
#include "ports/Fragments.h"
#include "ports/Ledger.h"
#include <AK/JsonObject.h>
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
#include <limits>
#include <memory>
#include <signal.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
            return vm.throw_completion<JS::TypeError>("Invalid utility process string"_utf16);
        auto bytes = MUST(value.as_string().utf16_string_view().to_byte_string());
        if (bytes.contains('\0'))
            return vm.throw_completion<JS::TypeError>("NUL in utility process string"_utf16);
        return std::string(bytes.characters(), bytes.length());
    }

    JS::ThrowCompletionOr<std::vector<std::string>> strings(JS::VM& vm, JS::Value value)
    {
        if (!value.is_object() || !is<JS::Array>(value.as_object()))
            return vm.throw_completion<JS::TypeError>("Invalid utility process arguments"_utf16);
        auto& array = value.as_object();
        auto size = TRY(TRY(array.get("length"_utf16)).to_u32(vm));
        if (size > 65536)
            return vm.throw_completion<JS::RangeError>("Too many utility process arguments"_utf16);
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
        uv_timer_t fragment_timer {};
        bool fragment_timer_initialized {};
        uint32_t fragment_serial {};
        Ports::Reassembler fragments;
        std::unordered_set<uint32_t> fragment_routes;
        size_t handles {}, writes {}, write_bytes {};
        unsigned diagnostic_count {}, control_diagnostic_count {};
        bool process_initialized {}, timer_initialized {}, started {}, announced {}, exited {},
            killed {}, channel_closed {};
        std::vector<char> incoming;
        std::string service_name;
        Ports::Ledger ports;
    };

    struct Service {
        void visit_capture_edges(GC::Cell::Visitor& visitor) const
        {
            // Shutdown retires all roots before the loop/VM can be destroyed.
            if (stopped)
                return;
            visitor.visit(realm);
            for (auto const& [id, endpoint] : endpoints) {
                (void)id;
                visitor.visit(endpoint->callback.ptr());
            }
        }

        JS::VM& vm;
        JS::Realm& realm;
        Mite::EventLoop& loop;
        bool child_side {}, stopped {};
        unsigned terminal_diagnostic_count {};
        uint64_t next_id { 1 };
        std::unordered_map<uint64_t, Endpoint*> endpoints;

        ~Service()
        {
            VERIFY(endpoints.empty());
        }

        // Fixed native counters only: never inspect payloads or environment contents.
        void diagnose(Endpoint& endpoint, char const* phase, size_t bytes = 0)
        {
            if (!std::getenv("FIREFLY_IPC_BYTES_DIAGNOSTIC") || endpoint.diagnostic_count >= 32)
                return;
            ++endpoint.diagnostic_count;
            fprintf(stderr,
                "FIREFLY_UTILITY_PHASE pid=%d side=%s endpoint=%llu phase=%s bytes=%zu count=%u\n",
                static_cast<int>(uv_os_getpid()), child_side ? "child" : "main",
                static_cast<unsigned long long>(endpoint.id), phase, bytes,
                endpoint.diagnostic_count);
        }

        void diagnose_control(Endpoint& endpoint, char const* direction, unsigned kind)
        {
            if (!std::getenv("FIREFLY_IPC_BYTES_DIAGNOSTIC")
                || endpoint.control_diagnostic_count >= 64)
                return;
            ++endpoint.control_diagnostic_count;
            fprintf(stderr,
                "FIREFLY_UTILITY_CONTROL pid=%d side=%s endpoint=%llu direction=%s kind=%u "
                "count=%u\n",
                static_cast<int>(uv_os_getpid()), child_side ? "child" : "main",
                static_cast<unsigned long long>(endpoint.id), direction, kind,
                endpoint.control_diagnostic_count);
        }

        void diagnose_terminal(
            Endpoint& endpoint, char const* reason, int status = 0, unsigned kind = 0)
        {
            if (!std::getenv("FIREFLY_IPC_BYTES_DIAGNOSTIC") || terminal_diagnostic_count >= 64)
                return;
            ++terminal_diagnostic_count;
            fprintf(stderr,
                "FIREFLY_UTILITY_TERMINAL pid=%d side=%s endpoint=%llu reason=%s status=%d kind=%u "
                "count=%u\n",
                static_cast<int>(uv_os_getpid()), child_side ? "child" : "main",
                static_cast<unsigned long long>(endpoint.id), reason, status, kind,
                terminal_diagnostic_count);
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
                        error(endpoint, status, "read-start-failure");
                }
        }

        void close_fragment_timer(Endpoint& endpoint)
        {
            auto* handle = reinterpret_cast<uv_handle_t*>(&endpoint.fragment_timer);
            if (!endpoint.fragment_timer_initialized || uv_is_closing(handle))
                return;
            uv_timer_stop(&endpoint.fragment_timer);
            uv_close(handle, [](uv_handle_t* handle) {
                auto& endpoint = *static_cast<Endpoint*>(handle->data);
                endpoint.owner.released(endpoint);
            });
        }

        int watch_fragments(Endpoint& endpoint)
        {
            if (!endpoint.fragment_timer_initialized) {
                auto status = uv_timer_init(&loop.native_loop(), &endpoint.fragment_timer);
                if (status)
                    return status;
                endpoint.fragment_timer_initialized = true;
                endpoint.fragment_timer.data = &endpoint;
                ++endpoint.handles;
                uv_unref(reinterpret_cast<uv_handle_t*>(&endpoint.fragment_timer));
            }

            if (uv_is_active(reinterpret_cast<uv_handle_t*>(&endpoint.fragment_timer)))
                return 0;
            return uv_timer_start(
                &endpoint.fragment_timer,
                [](uv_timer_t* timer) {
                    auto& endpoint = *static_cast<Endpoint*>(timer->data);
                    auto& owner = endpoint.owner;
                    if (!owner.stopped && !endpoint.channel_closed && endpoint.fragments.expired())
                        owner.error(endpoint, UV_ETIMEDOUT, "fragment-timeout");
                },
                100, 100);
        }

        void close_channel(
            Endpoint& endpoint, char const* reason = "local-api-close", int status = 0)
        {
            if (endpoint.channel_closed)
                return;
            diagnose_terminal(endpoint, reason, status);
            endpoint.channel_closed = true;
            endpoint.incoming.clear();
            endpoint.fragments.clear();
            endpoint.fragment_routes.clear();
            close_fragment_timer(endpoint);
            endpoint.ports.clear();
            close(endpoint.pipes[3]);
            emit(endpoint, "close");
        }

        void error(Endpoint& endpoint, int status, char const* reason = "native-error")
        {
            emit(endpoint, "error", text(vm, uv_strerror(status)));
            close_channel(endpoint, reason, status);
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
                                    count == UV_EOF ? UV_EPROTO : static_cast<int>(count),
                                    count == UV_EOF ? "invalid-frame" : "read-failure");
                            else
                                owner.close_channel(endpoint, "read-eof");
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
                    error(endpoint, UV_EMSGSIZE, "invalid-frame");
                    return;
                }

                if (endpoint.incoming.size() < length + 4)
                    return;
                std::vector<char> payload(
                    endpoint.incoming.begin() + 4, endpoint.incoming.begin() + 4 + length);
                endpoint.incoming.erase(
                    endpoint.incoming.begin(), endpoint.incoming.begin() + 4 + length);
                diagnose(endpoint, "frame-receive", payload.size());
                if (Ports::has_magic(payload)) {
                    receive_port(endpoint, payload);
                } else {
                    bytes(
                        endpoint, child_side ? "message" : "data", payload.data(), payload.size());
                }
            }
        }

        void receive_port(Endpoint& endpoint, std::span<char const> wire)
        {
            Ports::Packet packet {};
            if (!Ports::decode(wire, packet)) {
                error(endpoint, UV_EPROTO, "invalid-frame");
                return;
            }

            diagnose_control(endpoint, "receive", static_cast<unsigned>(packet.kind));
            if (packet.kind == Ports::Kind::Transfer) {
                if (!child_side || !endpoint.ports.admit_transfer(packet.ports)) {
                    error(endpoint, UV_EPROTO, "invalid-frame");
                    return;
                }

                bytes(endpoint, "port-transfer", wire.data(), wire.size());
                return;
            }

            auto state = endpoint.ports.state(packet.route);
            if (state == Ports::Ledger::State::Retired)
                return; // An already queued message cannot resurrect a retired capability.
            if (state != Ports::Ledger::State::Live) {
                error(endpoint, UV_EPROTO, "invalid-frame");
                return;
            }

            if (packet.kind == Ports::Kind::Fragment) {
                auto assembled = endpoint.fragments.accept(packet, endpoint.ports);
                if (assembled.status == Ports::Reassembler::Status::Invalid) {
                    error(endpoint, UV_EPROTO, "invalid-fragment");
                    return;
                }

                if (assembled.status == Ports::Reassembler::Status::Pending) {
                    endpoint.fragment_routes.insert(packet.route.id);
                    auto status = watch_fragments(endpoint);
                    if (status)
                        error(endpoint, status, "fragment-timer-failure");
                    return;
                }

                endpoint.fragment_routes.erase(packet.route.id);
                if (endpoint.fragment_routes.empty() && endpoint.fragment_timer_initialized)
                    uv_timer_stop(&endpoint.fragment_timer);
                // Trusted local envelope, never sent as an oversized physical frame.
                // accept() reserved the complete logical RX size before allocation.
                auto complete
                    = Ports::encode(Ports::Kind::Data, packet.route, {}, assembled.payload);
                bytes(endpoint, "port-data", complete.data(), complete.size());
                return;
            }

            if ((packet.kind == Ports::Kind::Data || packet.kind == Ports::Kind::Close)
                && endpoint.fragments.active(packet.route)) {
                error(endpoint, UV_EPROTO, "interleaved-fragment");
                return;
            }

            if (packet.kind == Ports::Kind::Ack) {
                if (!endpoint.ports.acknowledge(packet.route, Ports::read_size(packet.payload))) {
                    error(endpoint, UV_EPROTO, "invalid-frame");
                    return;
                }

                emit(endpoint, "port-writable");
            } else if (packet.kind == Ports::Kind::Close) {
                diagnose_terminal(
                    endpoint, "peer-port-close", 0, static_cast<unsigned>(packet.kind));
                endpoint.fragments.close(packet.route);
                endpoint.ports.close(packet.route);
                bytes(endpoint, "port-close", wire.data(), wire.size());
            } else {
                if (!endpoint.ports.received(packet.route, packet.payload.size())) {
                    error(endpoint, UV_ENOBUFS, "invalid-frame");
                    return;
                }

                bytes(endpoint, "port-data", wire.data(), wire.size());
            }
        }

        JS::ThrowCompletionOr<std::span<char const>> payload(JS::VM& vm, JS::Value value)
        {
            if (!value.is_object() || !JS::Uint8Array::is_engine_class_of(value.as_object())
                || JS::host_class_of(value.as_object())
                || &value.as_object().shape().realm() != &realm)
                return vm.throw_completion<JS::TypeError>(
                    "Utility channel requires its own Uint8Array"_utf16);
            auto& array = static_cast<JS::Uint8Array&>(value.as_object());
            auto* backing = array.viewed_array_buffer();
            if (!backing || backing->is_detached() || backing->is_shared_array_buffer()
                || !backing->is_fixed_length())
                return vm.throw_completion<JS::TypeError>(
                    "Utility channel requires fixed nonshared storage"_utf16);
            auto witness = JS::make_typed_array_with_buffer_witness_record(
                array, JS::ArrayBuffer::Order::SeqCst);
            if (JS::is_typed_array_out_of_bounds(witness))
                return vm.throw_completion<JS::TypeError>("Detached utility message"_utf16);
            auto size = JS::typed_array_byte_length(witness);
            if (!size)
                return std::span<char const> {};
            return std::span<char const>(
                reinterpret_cast<char const*>(
                    array.viewed_array_buffer()->data_at(array.byte_offset())),
                size);
        }

        JS::ThrowCompletionOr<JS::Value> send(JS::VM& vm, Endpoint* endpoint, JS::Value value)
        {
            auto wire = TRY(payload(vm, value));
            return JS::Value(write(endpoint, wire));
        }

        int write(Endpoint* endpoint, std::span<char const> wire)
        {
            if (stopped || !endpoint || endpoint->channel_closed || endpoint->exited)
                return UV_EPIPE;
            auto size = wire.size();
            if (size > frame_limit || (Ports::has_magic(wire) && size > Ports::physical_limit))
                return UV_EMSGSIZE;
            if (endpoint->writes >= write_count_limit
                || endpoint->write_bytes + size + 4 > write_bytes_limit)
                return UV_ENOBUFS;

            struct Write {
                Endpoint& endpoint;
                uv_write_t request {};
                std::vector<char> bytes;
            };

            auto* write = new Write { *endpoint, {}, std::vector<char>(size + 4) };
            for (size_t i = 0; i < 4; ++i)
                write->bytes[i] = static_cast<char>(size >> ((3 - i) * 8));
            if (size)
                memcpy(write->bytes.data() + 4, wire.data(), size);
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
                    if (!endpoint.owner.stopped && !endpoint.channel_closed) {
                        if (status)
                            endpoint.owner.error(endpoint, status, "write-failure");
                        else
                            endpoint.owner.emit(endpoint, "port-writable");
                    }
                });
            if (status)
                delete write;
            else {
                ++endpoint->writes;
                endpoint->write_bytes += size + 4;
                diagnose(*endpoint, "frame-send-admitted", size);
                if (Ports::has_magic(wire) && wire.size() >= Ports::header_size)
                    diagnose_control(*endpoint, "send", static_cast<unsigned char>(wire[4]));
            }

            return status;
        }

        static JS::ThrowCompletionOr<uint32_t> identity(JS::VM& vm, JS::Value value)
        {
            if (!value.is_number() || !std::isfinite(value.as_double()) || value.as_double() < 1
                || value.as_double() > Ports::identity_limit
                || std::trunc(value.as_double()) != value.as_double())
                return vm.throw_completion<JS::TypeError>(
                    "Invalid native utility port identity"_utf16);
            return static_cast<uint32_t>(value.as_double());
        }

        enum class PortAction { Send, Close, Consume };

        static char const* port_close_reason(JS::Value value)
        {
            // Private JS diagnostic enum only; never stringify caller values.
            if (!value.is_int32())
                return "local-api-port-close";
            switch (value.as_i32()) {
            case 1:
                return "js-finalizer";
            case 2:
                return "js-sweep-owner-gone";
            case 3:
                return "js-pump-owner-gone";
            case 4:
                return "js-explicit-close";
            case 5:
                return "js-remote-peer-close";
            case 6:
                return "js-transport-disconnect";
            case 7:
                return "js-delivery-budget";
            case 8:
                return "js-decode-error";
            case 9:
                return "js-shutdown";
            case 10:
                return "js-discarded-transfer";
            case 11:
                return "js-adopt-rollback";
            default:
                return "local-api-port-close";
            }
        }

        JS::ThrowCompletionOr<JS::Value> port_action(JS::VM& vm, PortAction action)
        {
            size_t offset = child_side ? 0 : 1;
            auto* endpoint = get(child_side ? JS::Value(1) : vm.argument(0));
            Ports::Identity route { TRY(identity(vm, vm.argument(offset))),
                TRY(identity(vm, vm.argument(offset + 1))) };
            if (stopped || !endpoint || endpoint->channel_closed || endpoint->exited)
                return JS::Value(UV_EPIPE);
            auto state = endpoint->ports.state(route);
            if (state == Ports::Ledger::State::Retired)
                return JS::Value(action == PortAction::Close ? 0 : UV_EPIPE);
            if (state != Ports::Ledger::State::Live)
                return JS::Value(UV_EINVAL);
            if (action == PortAction::Send) {
                auto data = TRY(payload(vm, vm.argument(offset + 2)));
                if (data.empty())
                    return JS::Value(UV_EINVAL);
                auto count = Ports::fragment_count(data.size());
                auto physical_bytes = Ports::fragment_wire_bytes(data.size()) + count * 4;
                if (!count)
                    return JS::Value(UV_EMSGSIZE);
                if (!endpoint->ports.can_send(route, data.size())
                    || count > write_count_limit - endpoint->writes
                    || physical_bytes > write_bytes_limit - endpoint->write_bytes)
                    return JS::Value(UV_ENOBUFS);
                uint32_t serial = 0;
                if (count > 1) {
                    if (endpoint->fragment_serial == std::numeric_limits<uint32_t>::max())
                        return JS::Value(UV_EOVERFLOW);
                    serial = ++endpoint->fragment_serial;
                }
                // Reserve one logical credit before the first physical write.
                // Entire physical batch capacity was checked without entering JS.
                endpoint->ports.sent(route, data.size());
                size_t offset = 0;
                for (;;) {
                    auto fragment = Ports::encode_next(route, serial, data, offset);
                    if (fragment.bytes.empty()) {
                        error(*endpoint, UV_EPROTO, "fragment-encode-failure");
                        return JS::Value(UV_EPROTO);
                    }

                    auto status = write(endpoint, fragment.bytes);
                    if (status) {
                        error(*endpoint, status, "fragment-write-failure");
                        return JS::Value(status);
                    }

                    if (fragment.complete)
                        return JS::Value(0);
                    offset = fragment.next_offset;
                }
            }

            if (action == PortAction::Close) {
                auto frame = Ports::encode(Ports::Kind::Close, route, {}, {});
                auto status = write(endpoint, frame);
                if (!status) {
                    diagnose_terminal(*endpoint, port_close_reason(vm.argument(offset + 2)), 0,
                        static_cast<unsigned>(Ports::Kind::Close));
                    endpoint->fragments.close(route);
                    endpoint->fragment_routes.erase(route.id);
                    if (endpoint->fragment_routes.empty() && endpoint->fragment_timer_initialized)
                        uv_timer_stop(&endpoint->fragment_timer);
                    endpoint->ports.close(route);
                }

                return JS::Value(status);
            }

            auto amount = vm.argument(offset + 2);
            if (!amount.is_number() || !std::isfinite(amount.as_double()) || amount.as_double() < 0
                || amount.as_double() > Ports::logical_limit
                || std::trunc(amount.as_double()) != amount.as_double())
                return vm.throw_completion<JS::TypeError>(
                    "Invalid native utility port consumption"_utf16);
            auto size = static_cast<uint32_t>(amount.as_double());
            if (!endpoint->ports.can_consume(route, size))
                return JS::Value(UV_EINVAL);
            auto encoded_size = Ports::size_payload(size);
            auto frame = Ports::encode(Ports::Kind::Ack, route, {}, encoded_size);
            auto status = write(endpoint, frame);
            if (!status)
                endpoint->ports.consumed(route, size);
            return JS::Value(status);
        }

        JS::ThrowCompletionOr<JS::Value> transfer(JS::VM& vm)
        {
            auto* endpoint = get(vm.argument(0));
            auto wire = TRY(payload(vm, vm.argument(1)));
            auto count_value = vm.argument(2);
            if (!count_value.is_number() || !std::isfinite(count_value.as_double())
                || count_value.as_double() < 1 || count_value.as_double() > 16
                || std::trunc(count_value.as_double()) != count_value.as_double())
                return vm.throw_completion<JS::TypeError>(
                    "Invalid utility port transfer count"_utf16);
            auto count = static_cast<size_t>(count_value.as_double());
            std::vector<Ports::Identity> identities;
            int status = stopped || !endpoint || endpoint->channel_closed || endpoint->exited
                ? UV_EPIPE
                : 0;
            if (!status && !endpoint->ports.prepare_transfer(count, identities))
                status = UV_EMFILE;
            if (!status && wire.empty())
                status = UV_EINVAL;
            if (!status && wire.size() > Ports::physical_limit - Ports::header_size - count * 8)
                status = UV_EMSGSIZE;
            // Allocate every destination descriptor before write admission. No
            // application code runs between admission and the ledger commit.
            GC::Root ports { TRY(JS::Array::create(realm, status ? 0 : count)) };
            if (!status) {
                for (size_t index = 0; index < count; ++index) {
                    GC::Root descriptor { JS::Object::create(realm, nullptr) };
                    descriptor->define_direct_property(
                        "id"_utf16, JS::Value(identities[index].id), JS::default_attributes);
                    descriptor->define_direct_property("generation"_utf16,
                        JS::Value(identities[index].generation), JS::default_attributes);
                    ports->define_direct_property(index, descriptor.ptr(), JS::default_attributes);
                }
            }

            GC::Root result { JS::Object::create(realm, nullptr) };
            result->define_direct_property("ports"_utf16, ports.ptr(), JS::default_attributes);
            if (!status) {
                auto frame = Ports::encode(Ports::Kind::Transfer, {}, identities, wire);
                status = write(endpoint, frame);
                if (!status) {
                    auto admitted = endpoint->ports.admit_transfer(identities);
                    VERIFY(admitted);
                } else {
                    ports = TRY(JS::Array::create(realm, 0));
                    result->define_direct_property(
                        "ports"_utf16, ports.ptr(), JS::default_attributes);
                }
            }

            result->define_direct_property(
                "status"_utf16, JS::Value(status), JS::default_attributes);
            return JS::Value(result.ptr());
        }

        void shutdown()
        {
            if (stopped)
                return;
            stopped = true;
            for (auto& [id, endpoint] : endpoints) {
                if (!endpoint->channel_closed)
                    diagnose_terminal(*endpoint, "shutdown");
                endpoint->callback = nullptr;
                endpoint->channel_closed = true;
                endpoint->fragments.clear();
                endpoint->fragment_routes.clear();
                close_fragment_timer(*endpoint);
                endpoint->ports.clear();
                close_startup(*endpoint);
                for (auto& pipe : endpoint->pipes)
                    close(pipe);
                if (endpoint->started && !endpoint->exited)
                    uv_process_kill(&endpoint->process, SIGKILL);
            }
        }
    };

    void install_ports(JS::Realm& realm, JS::Object& host, std::shared_ptr<Service> owner)
    {
        host.define_direct_property(
            "portQueueFull"_utf16, JS::Value(UV_ENOBUFS), JS::default_attributes);
        for (auto const& [name, action] :
            std::array { std::pair { "sendPort"_utf16, Service::PortAction::Send },
                std::pair { "closePort"_utf16, Service::PortAction::Close },
                std::pair { "consumePort"_utf16, Service::PortAction::Consume } }) {
            host.define_native_function(realm, name,
                GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(owner), action),
                    [](auto& capture, auto& action, JS::VM& vm) {
                        return capture.value->port_action(vm, action);
                    }),
                static_cast<int>(owner->child_side ? 3 : 4), JS::default_attributes);
        }

        if (!owner->child_side) {
            host.define_native_function(realm, "sendTransfer"_utf16,
                GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(owner)),
                    [](auto& capture, JS::VM& vm) { return capture.value->transfer(vm); }),
                3, JS::default_attributes);
        }
    }

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
    install_ports(realm, *host, owner);
    host->define_native_function(realm, "spawn"_utf16,
        GC::create_function(realm.heap(),
            GC::captures(Mite::shared_capture(owner), realm, electron_compat),
            [](auto& owner_capture, auto& realm, auto& electron_compat,
                JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& owner = owner_capture.value;
                if (owner->stopped)
                    return vm.throw_completion<JS::Error>("Utility service stopped"_utf16);
                if (!vm.argument(1).is_function())
                    return vm.throw_completion<JS::TypeError>(
                        "Utility callback must be callable"_utf16);
                auto spec = TRY(vm.argument(0).to_object(vm));
                std::array<char, 32768> executable {};
                size_t executable_size = executable.size();
                if (uv_exepath(executable.data(), &executable_size))
                    return vm.throw_completion<JS::Error>(
                        "Cannot resolve utility executable"_utf16);
                std::string file(executable.data(), executable_size);
                auto entry = TRY(string(vm, TRY(spec->get("entry"_utf16))));
                auto args = TRY(strings(vm, TRY(spec->get("args"_utf16))));
                std::vector<std::string> exec_argv;
                auto flags = TRY(spec->get("execArgv"_utf16));
                if (!flags.is_undefined()) {
                    if (!flags.is_object() || !is<JS::Array>(flags.as_object()))
                        return vm.throw_completion<JS::TypeError>("Invalid utility execArgv"_utf16);
                    auto& array = flags.as_object();
                    auto length = TRY(TRY(array.get("length"_utf16)).to_u32(vm));
                    auto flag = length == 1 ? TRY(array.get(0)) : JS::js_undefined();
                    if (length > 1
                        || (length == 1
                            && (!flag.is_string()
                                || flag.as_string().utf16_string_view() != "--expose-gc"_utf16))) {
                        GC::Root error { JS::Error::create(
                            realm, "Unsupported utility engine command-line flag"_utf16) };
                        error->define_direct_property("code"_utf16,
                            JS::PrimitiveString::create(vm, "ERR_FIREFLY_UNSUPPORTED_API"_utf16),
                            JS::default_attributes);
                        return JS::throw_completion(error);
                    }

                    if (length)
                        exec_argv.emplace_back("--expose-gc");
                }

                auto env = TRY(strings(vm, TRY(spec->get("env"_utf16))));
                auto cwd = TRY(string(vm, TRY(spec->get("cwd"_utf16))));
                auto name_value = TRY(spec->get("serviceName"_utf16));
                auto name = name_value.is_undefined() ? std::string("Node Utility Process")
                                                      : TRY(string(vm, name_value));
                if (name.empty())
                    name = "Node Utility Process";
                if (name.size() > 4096
                    || String::from_utf8(StringView(name.data(), name.size())).is_error())
                    return vm.throw_completion<JS::TypeError>("Invalid utility service name"_utf16);
                auto stdio = TRY(TRY(spec->get("stdio"_utf16)).to_object(vm));
                std::array<std::string, 3> modes;
                for (size_t i = 0; i < 3; ++i) {
                    modes[i] = TRY(string(vm, TRY(stdio->get(i))));
                    if ((i == 0 && modes[i] != "ignore")
                        || (i != 0 && modes[i] != "ignore" && modes[i] != "pipe"
                            && modes[i] != "inherit"))
                        return vm.throw_completion<JS::TypeError>(
                            "Unsupported utility stdio mode"_utf16);
                }
                // Recheck after accessors: no user code executes between this check and spawn.
                if (owner->stopped || owner->endpoints.size() >= child_limit || file.empty()
                    || entry.empty())
                    return vm.throw_completion<JS::Error>(
                        "Utility process unavailable or limit exceeded"_utf16);
                exec_argv.insert(exec_argv.begin(),
                    { file, electron_compat ? "--firefly-utility-electron" : "--firefly-utility" });
                exec_argv.push_back(entry);
                args.insert(args.begin(), exec_argv.begin(), exec_argv.end());
                auto arguments = pointers(args), environment = pointers(env);
                auto* endpoint = new Endpoint { *owner, owner->next_id++,
                    GC::Root { vm.argument(1).as_function() } };
                endpoint->service_name = move(name);
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
#ifdef _WIN32
                        // Preserve our raw framing while allowing concurrent child reads/writes.
                        // ipc=1 would also select overlapped I/O, but adds Node handle framing.
                        if (i == 3)
                            handles[i].flags
                                = static_cast<uv_stdio_flags>(handles[i].flags | UV_NONBLOCK_PIPE);
#endif
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
                        // fd3 may still contain ordered messages from before exit.
                        // Retired capabilities ignore those late frames without
                        // inventing a new protocol failure for a reaped child.
                        endpoint.ports.retire_all();
                        endpoint.owner.announce(endpoint);
                        // Preserve the native identity before the process handle is
                        // closed. Service names are admission-time snapshots, not
                        // mutable process.title or an invented metrics sample.
                        if (signal || code != 0) {
                            JsonObject details;
                            details.set("type"sv, "Utility"sv);
                            details.set("name"sv,
                                MUST(String::from_utf8(StringView(
                                    endpoint.service_name.data(), endpoint.service_name.size()))));
                            details.set("pid"sv, static_cast<i64>(process->pid));
                            details.set("exitCode"sv, signal ? -signal : code);
                            bool crash = signal == SIGABRT || signal == SIGSEGV || signal == SIGILL
                                || signal == SIGFPE;
#ifndef _WIN32
                            crash = crash || signal == SIGBUS;
#endif
                            auto reason
                                = signal ? (crash ? "crashed"sv : "killed"sv) : "abnormal-exit"sv;
                            details.set("reason"sv, reason);
                            auto encoded = details.serialized().to_byte_string();
                            endpoint.owner.emit(
                                endpoint, "gone", text(endpoint.owner.vm, encoded.characters()));
                        }

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
            }),
        2, JS::default_attributes);
    host->define_native_function(realm, "send"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(owner)),
            [](auto& owner_capture, JS::VM& vm) {
                auto& owner = owner_capture.value;
                return owner->send(vm, owner->get(vm.argument(0)), vm.argument(1));
            }),
        2, JS::default_attributes);
    host->define_native_function(realm, "kill"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(owner)),
            [](auto& owner_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& owner = owner_capture.value;
                auto* endpoint = owner->get(vm.argument(0));
                if (owner->stopped || !endpoint || !endpoint->started || endpoint->exited
                    || endpoint->killed)
                    return JS::Value(false);
                endpoint->killed = uv_process_kill(&endpoint->process, SIGTERM) == 0;
                return JS::Value(endpoint->killed);
            }),
        1, JS::default_attributes);
    host->define_native_function(realm, "shutdown"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(owner)),
            [](auto& owner_capture, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                auto& owner = owner_capture.value;
                owner->shutdown();
                return JS::js_undefined();
            }),
        0, JS::default_attributes);
    return *host;
}

JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_parent_host(
    JS::VM& vm, JS::Realm& realm, Mite::EventLoop& loop, int fd)
{
    if (uv_guess_handle(fd) != UV_NAMED_PIPE)
        return vm.throw_completion<JS::Error>("Missing inherited utility channel"_utf16);
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

        return vm.throw_completion<JS::Error>("Cannot open inherited utility channel"_utf16);
    }

    GC::Root host { JS::Object::create(realm, nullptr) };
    install_ports(realm, *host, owner);
    host->define_native_function(realm, "setHandler"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(owner)),
            [](auto& owner_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& owner = owner_capture.value;
                if (!vm.argument(0).is_function())
                    return vm.throw_completion<JS::TypeError>(
                        "Utility handler must be callable"_utf16);
                auto found = owner->endpoints.find(1);
                if (owner->stopped || found == owner->endpoints.end())
                    return vm.throw_completion<JS::Error>("Utility channel closed"_utf16);
                found->second->callback = vm.argument(0).as_function();
                owner->diagnose(*found->second, "parent-handler-installed");
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    host->define_native_function(realm, "send"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(owner)),
            [](auto& owner_capture, JS::VM& vm) {
                auto& owner = owner_capture.value;
                return owner->send(vm, owner->get(JS::Value(1)), vm.argument(0));
            }),
        1, JS::default_attributes);
    host->define_native_function(realm, "close"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::shared_capture(owner)),
            [](auto& owner_capture, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                auto& owner = owner_capture.value;
                if (auto* endpoint = owner->get(JS::Value(1)))
                    owner->close_channel(*endpoint);
                return JS::js_undefined();
            }),
        0, JS::default_attributes);
    return GC::Ref<JS::Object> { *host };
}
}
