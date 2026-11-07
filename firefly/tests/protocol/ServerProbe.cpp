/** Real two-process resource requests: the native parent alone owns authorized file paths. */
#include "../ipc/SpawnBytePeer.h"
#include "Arguments.h"
#include "native/protocol/files/Server.h"
#include <AK/Array.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/EventLoop.h>
#include <LibCore/System.h>
#include <LibIPC/StartupTransport.h>
#ifdef AK_OS_WINDOWS
#include <LibIPC/HandleBroker.h>
#include <fcntl.h>
#include <io.h>
#endif
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

namespace P = Firefly::Protocols;
using Channel = Firefly::Bytes::Channel;

namespace {
struct State final : RefCounted<State> {
    bool active { true }, bad { false }, closed { false };
    Array<bool, 4> headers {}, done {};
    Array<u64, 4> received {};
};

ErrorOr<void> until(Function<bool()> ready, int seconds)
{
    Core::ElapsedTimer elapsed;
    elapsed.start();
    while (!ready()) {
        Core::EventLoop::current().pump(Core::EventLoop::WaitMode::PollForEvents);
        if (elapsed.elapsed_milliseconds() > seconds * 1000)
            return Error::from_string_literal("Resource channel test timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return {};
}

ErrorOr<int> child(StringView bootstrap, StringView mode)
{
    // The native transport bootstrap is synchronous. Bound it independently
    // from event-loop timers; this watchdog never touches a desktop window.
    std::mutex lock;
    std::condition_variable wake;
    bool finished = false;
    std::thread watchdog([&] {
        std::unique_lock guard(lock);
        if (!wake.wait_for(guard, std::chrono::seconds(45), [&] { return finished; }))
            Core::Process::terminate_immediately(1);
    });
    ScopeGuard joined = [&] {
        {
            std::lock_guard guard(lock);
            finished = true;
        }

        wake.notify_one();
        watchdog.join();
    };

    auto state = adopt_ref(*new State);
    Core::ElapsedTimer lifetime;
    lifetime.start();
    RefPtr<Channel> channel;
    auto request = [&](P::FileRequestKind kind, u32 id, u64 credit = 0, StringView url = {},
                       u64 epoch = 101) -> ErrorOr<void> {
        auto wire = TRY(P::encode_file_request({ kind, epoch, id, credit, url.bytes() }));
        return channel->send(wire);
    };

    bool disconnect = mode.is_one_of("malformed"sv, "epoch"sv, "replay"sv, "overcredit"sv,
        "future-credit"sv, "quota"sv, "deadline"sv, "revoke"sv, "reentrant"sv);
    channel = TRY(Channel::create(TRY(IPC::take_over_startup_transport(bootstrap)),
        {
            .on_data =
                [&, state](ReadonlyBytes wire) {
                    if (!state->active)
                        return;
                    auto decoded = P::decode_file_packet(wire);
                    if (decoded.is_error()) {
                        state->bad = true;
                        return;
                    }

                    auto const& packet = decoded.value();
                    auto id = packet.request;
                    if (packet.epoch != 101 || id > 3 || state->done[id]) {
                        state->bad = true;
                        return;
                    }

                    if (packet.kind == P::FilePacketKind::Cancel) {
                        if (!((mode == "multiplex"sv && id == 3) || mode == "cancel"sv))
                            state->bad = true;
                        state->done[id] = true;
                        return;
                    }

                    if (packet.kind == P::FilePacketKind::Metadata) {
                        auto head = P::decode_file_response_head(
                            { reinterpret_cast<char const*>(packet.payload.data()),
                                packet.payload.size() });
                        if (state->headers[id] || head.is_error() || head.value().request != id
                            || head.value().content_length != 4194323
                            || head.value().head != (id == 2)) {
                            state->bad = true;
                            return;
                        }

                        state->headers[id] = true;
                        if (mode == "peerloss"sv) {
                            channel->close();
                            return;
                        }

                        if (mode == "overcredit"sv) {
                            if (request(P::FileRequestKind::Credit, id, P::file_chunk_limit)
                                    .is_error()
                                || request(P::FileRequestKind::Credit, id, P::file_chunk_limit)
                                    .is_error())
                                state->bad = true;
                        } else if (id == 1 && mode != "deadline"sv && mode != "body"sv
                            && mode != "kill"sv && mode != "revoke"sv) {
                            if (request(P::FileRequestKind::Credit, id, P::file_chunk_limit)
                                    .is_error())
                                state->bad = true;
                        }

                        return;
                    }

                    if (!state->headers[id]
                        || (id != 1 && packet.kind == P::FilePacketKind::Chunk)) {
                        state->bad = true;
                        return;
                    }

                    if (packet.kind == P::FilePacketKind::Complete) {
                        if (packet.offset != state->received[id]
                            || state->received[id] != (id == 2 ? 0 : 4194323))
                            state->bad = true;
                        state->done[id] = true;
                        return;
                    }

                    if (packet.kind != P::FilePacketKind::Chunk
                        || packet.offset != state->received[id]) {
                        state->bad = true;
                        return;
                    }

                    for (size_t i = 0; i < packet.payload.size(); ++i)
                        if (packet.payload[i] != static_cast<u8>((packet.offset + i) * 37 + 11))
                            state->bad = true;
                    state->received[id] += packet.payload.size();
                    auto sent = mode == "cancel"sv
                        ? request(P::FileRequestKind::Cancel, id)
                        : request(P::FileRequestKind::Credit, id, packet.payload.size());
                    if (sent.is_error())
                        state->bad = true;
                },
            .on_writable = {},
            .on_closed =
                [state](auto) {
                    if (state->active)
                        state->closed = true;
                },
        }));
    ScopeGuard close = [&] {
        state->active = false;
        channel->close();
    };

    if (mode == "malformed"sv)
        TRY(channel->send("bad"sv.bytes()));
    else if (mode == "future-credit"sv)
        TRY(request(P::FileRequestKind::Credit, 1, 1));
    else {
        TRY(request(
            P::FileRequestKind::Get, 1, 0, "app://bundle/body"sv, mode == "epoch"sv ? 102 : 101));
        if (mode == "multiplex"sv) {
            TRY(request(P::FileRequestKind::Head, 2, 0, "app://bundle/body"sv));
            TRY(request(P::FileRequestKind::Get, 3, 0, "file:///untrusted-path"sv));
        } else if (mode == "replay"sv)
            TRY(request(P::FileRequestKind::Get, 1, 0, "app://bundle/body"sv));
        else if (mode == "quota"sv)
            for (u32 id = 2; id <= 9; ++id)
                TRY(request(P::FileRequestKind::Get, id, 0, "app://bundle/body"sv));
    }

    if (mode == "body"sv) {
        TRY(until([&] { return state->bad || state->closed || state->headers[1]; }, 10));
        Core::ElapsedTimer delay;
        delay.start();
        TRY(until(
            [&] { return state->bad || state->closed || delay.elapsed_milliseconds() >= 50; }, 10));
        if (state->received[1])
            return Error::from_string_literal("Server read body without consumer credit");
        TRY(request(P::FileRequestKind::Credit, 1, P::file_chunk_limit));
    }

    TRY(until(
        [&] {
            return state->bad || state->closed
                || (!disconnect && state->done[1]
                    && (mode != "multiplex"sv || (state->done[2] && state->done[3])));
        },
        mode == "deadline"sv ? 40 : 10));
    if (state->bad || (disconnect && !state->closed))
        return Error::from_string_literal("Resource channel response/state mismatch");
    if (mode == "deadline"sv
        && (!state->headers[1] || state->received[1] || lifetime.elapsed_milliseconds() < 29000))
        return Error::from_string_literal("Request deadline did not exercise a live paused file");
    if (mode == "cancel"sv && state->received[1] != P::file_chunk_limit)
        return Error::from_string_literal("Cancellation did not stop at credited body limit");
    if (!disconnect && mode != "peerloss"sv && !state->done[1])
        return Error::from_string_literal("Resource channel lost response before completion");
    TRY(until([&] { return !channel->pending_send_count() || state->closed; }, 10));
    return 0;
}

ErrorOr<void> parent(StringView path, StringView mode)
{
    auto executable = TRY(Core::System::current_executable_path());
    Array<ByteString, 1> extra { ByteString(mode) };
    auto peer = TRY(spawn_byte_peer(executable, extra));
    size_t calls = 0;
    RefPtr<P::FileServer> server;
    server = TRY(P::FileServer::create(
        move(peer->transport), 101, [&](StringView url, bool) -> ErrorOr<P::AuthorizedFile> {
            ++calls;
            if (mode == "reentrant"sv)
                server->revoke();
            if (url != "app://bundle/body"sv)
                return Error::from_string_literal("URL not authorized for fixture document");
            return P::AuthorizedFile { ByteString(path), { { "Cache-Control", "no-store" } } };
        }));
    ScopeGuard revoke = [&] { server->revoke(); };
    if (mode == "kill"sv || mode == "revoke"sv) {
        TRY(until([&] { return server->files() == 1 || server->is_revoked(); }, 10));
        if (server->files() != 1)
            return Error::from_string_literal("Peer-kill fixture has no pending checked file");
        if (mode == "kill"sv)
            TRY(Core::Process::terminate_process(
                peer->process.pid(), Core::Process::TerminationMode::Forceful));
        else
            server->revoke();
    }
    // Channel closure notification is deliberately deferred. Observe the owning
    // service's terminal transition before checking cleanup, not just the fd.
    TRY(until([&] { return server->is_revoked(); }, mode == "deadline"sv ? 40 : 15));
    if (server->pending() || server->files())
        return Error::from_string_literal("Disconnected resource service retained jobs/files");
    auto status = TRY(peer->wait());
    if ((mode == "kill"sv && status == 0) || (mode != "kill"sv && status != 0))
        return Error::from_string_literal("Resource consumer process failed");
    auto expected = mode == "multiplex"sv                             ? 3
        : mode == "quota"sv                                           ? 8
        : mode.is_one_of("malformed"sv, "epoch"sv, "future-credit"sv) ? 0
                                                                      : 1;
    if (calls != static_cast<size_t>(expected))
        return Error::from_string_literal("Resource admission callback count mismatch");
    outln("PASS native resource service {}: authorized URL requests, bounded credits and terminal "
          "file cleanup",
        mode);
    return {};
}
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    Firefly::Tests::ProtocolArguments native_arguments;
    TRY(native_arguments.initialize(arguments));
    arguments = native_arguments.arguments();
#ifdef AK_OS_WINDOWS
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1)
        return Error::from_string_literal("Cannot configure binary fixture streams");
#endif
    Core::EventLoop::initialize_for_current_thread();
    if (arguments.argc == 4 && arguments.strings[1] == "child"sv)
        return child(arguments.strings[2], arguments.strings[3]);
#ifdef AK_OS_WINDOWS
    IPC::HandleBroker::install(make<IPC::LocalHandleBroker>());
#endif
    if (arguments.argc != 3)
        return Error::from_string_literal("Expected service operation and fixture path");
    auto mode = arguments.strings[1];
    if (mode == "request-decode"sv) {
        std::string input((std::istreambuf_iterator<char>(std::cin)), {});
        auto request = TRY(
            P::decode_file_request({ reinterpret_cast<u8 const*>(input.data()), input.size() }));
        outln("{} {} {} {} {}", static_cast<u8>(request.kind), request.epoch, request.request,
            request.credit, request.url.size());
    } else if (mode == "request-encode"sv) {
        auto wire = TRY(P::encode_file_request({ P::FileRequestKind::Get, 0x0102030405060708ull,
            0x10203040, 0, "app://bundle/body"sv.bytes() }));
        if (fwrite(wire.data(), 1, wire.size(), stdout) != wire.size())
            return Error::from_string_literal("Cannot write request fixture");
    } else
        TRY(parent(arguments.strings[2], mode));
    return 0;
}
