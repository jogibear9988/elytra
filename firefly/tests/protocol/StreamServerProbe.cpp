/** Real two-process response producers: bounded credit and terminal ownership, no VM/GUI. */
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
#endif
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace P = Firefly::Protocols;
using Channel = Firefly::Bytes::Channel;
constexpr u64 body_length = 196631;

namespace {
ErrorOr<void> until(Function<bool()> ready)
{
    Core::ElapsedTimer timer;
    timer.start();
    while (!ready()) {
        Core::EventLoop::current().pump(Core::EventLoop::WaitMode::PollForEvents);
        if (timer.elapsed_milliseconds() > 10000)
            return Error::from_string_literal("Stream producer/consumer timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return {};
}

struct Consumer : RefCounted<Consumer> {
    bool bad {}, closed {}, active { true };
    Array<bool, 10> headers {}, done {};
    Array<u64, 10> received {};
};

ErrorOr<int> child(StringView bootstrap, StringView mode)
{
    std::mutex mutex;
    std::condition_variable wake;
    bool finished = false;
    std::thread watchdog([&] {
        std::unique_lock lock(mutex);
        if (!wake.wait_for(lock, std::chrono::seconds(20), [&] { return finished; }))
            Core::Process::terminate_immediately(1);
    });
    ScopeGuard joined = [&] {
        {
            std::lock_guard lock(mutex);
            finished = true;
        }

        wake.notify_one();
        watchdog.join();
    };

    auto state = adopt_ref(*new Consumer);
    RefPtr<Channel> channel;
    auto send = [&](P::FileRequestKind kind, u32 id, u64 credit = 0) -> ErrorOr<void> {
        auto url = kind == P::FileRequestKind::Get || kind == P::FileRequestKind::Head
            ? "app://owned/stream"sv
            : StringView {};
        auto wire = TRY(P::encode_file_request({ kind, 701, id, credit, url.bytes() }));
        return channel->send(wire);
    };

    bool suppressed = mode.is_one_of("head"sv, "bodyless"sv);
    bool failure = mode.is_one_of("error"sv, "oversize"sv, "short"sv, "cancel"sv, "stale"sv,
        "authorization-stale"sv, "pull-bound"sv);
    channel = TRY(Channel::create(TRY(IPC::take_over_startup_transport(bootstrap)),
        { .on_data =
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
                    if (packet.epoch != 701 || id < 1 || id > 9 || state->done[id]) {
                        state->bad = true;
                        return;
                    }

                    if (packet.kind == P::FilePacketKind::Cancel) {
                        if (!failure)
                            state->bad = true;
                        state->done[id] = true;
                        return;
                    }

                    if (packet.kind == P::FilePacketKind::Metadata) {
                        auto head = P::decode_stream_response_head(
                            { reinterpret_cast<char const*>(packet.payload.data()),
                                packet.payload.size() });
                        if (state->headers[id] || head.is_error() || head.value().request != id
                            || head.value().status != (mode == "bodyless"sv ? 204 : 201)
                            || head.value().head != (mode == "head"sv)
                            || head.value().body_null != suppressed
                            || (mode == "unknown"sv && head.value().content_length.has_value()))
                            state->bad = true;
                        state->headers[id] = true;
                        return;
                    }

                    if (!state->headers[id] || id != 1) {
                        state->bad = true;
                        return;
                    }

                    if (packet.kind == P::FilePacketKind::Complete) {
                        if (failure || mode == "revoke"sv || packet.offset != state->received[id]
                            || state->received[id] != (suppressed ? 0 : body_length))
                            state->bad = true;
                        state->done[id] = true;
                        return;
                    }

                    if (packet.kind != P::FilePacketKind::Chunk || suppressed
                        || packet.payload.is_empty() || packet.payload.size() > 4096
                        || packet.offset != state->received[id]) {
                        state->bad = true;
                        return;
                    }

                    for (size_t i = 0; i < packet.payload.size(); ++i)
                        if (packet.payload[i] != static_cast<u8>((packet.offset + i) * 29 + 7))
                            state->bad = true;
                    state->received[id] += packet.payload.size();
                    if (send(mode == "cancel"sv ? P::FileRequestKind::Cancel
                                                : P::FileRequestKind::Credit,
                            id, mode == "cancel"sv ? 0 : packet.payload.size())
                            .is_error())
                        state->bad = true;
                },
            .on_writable = {},
            .on_closed =
                [state](auto) {
                    if (state->active)
                        state->closed = true;
                } }));
    ScopeGuard closed = [&] {
        state->active = false;
        channel->close();
    };

    TRY(send(mode == "head"sv ? P::FileRequestKind::Head : P::FileRequestKind::Get, 1));
    if (mode == "quota"sv) {
        for (u32 id = 2; id <= 9; ++id)
            TRY(send(P::FileRequestKind::Get, id));
    } else if (mode == "authorization-stale"sv) {
        TRY(send(P::FileRequestKind::Cancel, 1));
    } else {
        TRY(until([&] { return state->bad || state->closed || state->headers[1]; }));
        if (!suppressed) {
            Core::ElapsedTimer paused;
            paused.start();
            TRY(until([&] {
                return state->bad || state->closed || paused.elapsed_milliseconds() >= 50;
            }));
            if (state->received[1])
                return Error::from_string_literal("Body arrived without consumer credit");
            TRY(send(P::FileRequestKind::Credit, 1, mode == "pull-bound"sv ? 1 : 4096));
            if (mode == "stale"sv || mode == "pull-bound"sv) {
                Core::ElapsedTimer wait;
                wait.start();
                TRY(until([&] {
                    return state->bad || state->closed || wait.elapsed_milliseconds() >= 50;
                }));
                TRY(send(
                    mode == "stale"sv ? P::FileRequestKind::Cancel : P::FileRequestKind::Credit, 1,
                    mode == "stale"sv ? 0 : 4095));
            }
        }
    }

    TRY(until([&] {
        if (state->bad || state->closed)
            return true;
        if (mode == "quota"sv)
            return false; // The ninth request revokes the entire owner channel, not one job.
        return state->done[1];
    }));
    if (state->bad || (mode.is_one_of("revoke"sv, "quota"sv) ? !state->closed : !state->done[1]))
        return Error::from_string_literal("Stream response body/terminal mismatch");
    if (mode == "cancel"sv && state->received[1] != 4096)
        return Error::from_string_literal("Cancel exceeded one credited chunk");
    TRY(until([&] { return state->closed || !channel->pending_send_count(); }));
    return 0;
}

ErrorOr<void> parent(StringView mode)
{
    auto peer = TRY(spawn_byte_peer(
        TRY(Core::System::current_executable_path()), Array<ByteString, 1> { ByteString(mode) }));
    RefPtr<P::FileServer> server;
    size_t admissions = 0, pulls = 0, cancellations = 0;
    bool bad = false;
    u64 offset = 0;
    Core::ElapsedTimer admission_timer;
    P::StreamCompletion late;
    P::FileServer::ResponseCompletion late_authorization;
    auto stream = [&](u32 id, bool head) -> P::AuthorizedStream {
        bool suppressed = mode.is_one_of("head"sv, "bodyless"sv);
        Optional<u64> length = mode.is_one_of("unknown"sv, "bodyless"sv)
            ? Optional<u64> {}
            : Optional<u64> { body_length + (mode == "short"sv ? 1 : 0) };
        return { { id, static_cast<u16>(mode == "bodyless"sv ? 204 : 201), length, head, suppressed,
                     { { "Content-Type", "application/octet-stream" } } },
            [&](size_t maximum, P::StreamCompletion done) {
                ++pulls;
                if (!maximum || maximum > 4096 || mode.is_one_of("head"sv, "bodyless"sv)
                    || admission_timer.elapsed_milliseconds() < 40)
                    bad = true;
                if (mode.is_one_of("stale"sv, "revoke"sv, "pull-bound"sv)) {
                    late = move(done);
                    if (mode == "revoke"sv)
                        Core::deferred_invoke([&] { server->revoke(); });
                    return;
                }
                // Actual asynchronous producer completion, never body accumulation.
                Core::deferred_invoke([&, maximum, done = move(done)]() mutable {
                    if (mode == "error"sv) {
                        if (!done(Error::from_errno(EIO)))
                            bad = true;
                        return;
                    }

                    auto count = mode == "oversize"sv ? maximum + 1
                                                      : min<u64>(maximum, body_length - offset);
                    auto bytes = MUST(ByteBuffer::create_uninitialized(count));
                    for (size_t i = 0; i < count; ++i)
                        bytes[i] = static_cast<u8>((offset + i) * 29 + 7);
                    offset += count;
                    if (!done(P::StreamChunk { move(bytes), offset == body_length }))
                        bad = true;
                    if (mode == "duplicate"sv && done(P::StreamChunk { {}, true }))
                        bad = true;
                });
            },
            [&] {
                ++cancellations;
                if (late) {
                    auto complete = move(late);
                    if (complete(P::StreamChunk { {}, true }))
                        bad = true;
                }
            } };
    };

    server = TRY(P::FileServer::create_responses(
        move(peer->transport), 701,
        [&](P::FileServer::AuthorizationRequest request, P::FileServer::ResponseCompletion done) {
            ++admissions;
            admission_timer.start();
            if (request.url != "app://owned/stream"sv || server->files())
                bad = true;
            if (mode == "authorization-stale"sv) {
                late_authorization = move(done);
                return;
            }

            if (!done(P::FileServer::AuthorizedResponse { stream(request.id, request.head) }))
                bad = true;
        },
        [&](u32 id) {
            if (late_authorization) {
                auto complete = move(late_authorization);
                if (complete(P::FileServer::AuthorizedResponse { stream(id, false) }))
                    bad = true;
            }
        }));
    ScopeGuard revoke = [&] { server->revoke(); };
    if (mode == "pull-bound"sv) {
        TRY(until([&] { return pulls == 1 || server->is_revoked(); }));
        // Give the child's second credit packet a real event-loop turn to arrive.
        Core::ElapsedTimer delay;
        delay.start();
        TRY(until([&] { return delay.elapsed_milliseconds() >= 150 || server->is_revoked(); }));
        auto complete = move(late);
        auto bytes = TRY(ByteBuffer::create_zeroed(2));
        if (!complete(P::StreamChunk { move(bytes), false }))
            bad = true;
    }

    TRY(until([&] { return server->is_revoked(); }));
    auto status = TRY(peer->wait());
    if (status || bad || server->pending() || server->files()
        || admissions != (mode == "quota"sv ? 8u : 1u)
        || ((mode.is_one_of("head"sv, "bodyless"sv, "quota"sv, "authorization-stale"sv)) && pulls)
        || (mode.is_one_of("head"sv, "bodyless"sv, "cancel"sv, "stale"sv, "revoke"sv, "error"sv,
                "oversize"sv, "short"sv, "pull-bound"sv, "authorization-stale"sv)
            && cancellations != 1)
        || (mode == "quota"sv && cancellations != 8)
        || (mode.is_one_of("known"sv, "unknown"sv, "duplicate"sv) && cancellations))
        return Error::from_string_literal("Stream producer ownership/credit/cancellation mismatch");
    outln("PASS real two-process stream server {}: demand, checked frames and retired producer "
          "ownership",
        mode);
    return {};
}
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    Firefly::Tests::ProtocolArguments native_arguments;
    TRY(native_arguments.initialize(arguments));
    arguments = native_arguments.arguments();
    Core::EventLoop::initialize_for_current_thread();
    if (arguments.argc == 4 && arguments.strings[1] == "child"sv)
        return child(arguments.strings[2], arguments.strings[3]);
#ifdef AK_OS_WINDOWS
    IPC::HandleBroker::install(make<IPC::LocalHandleBroker>());
#endif
    if (arguments.argc != 2)
        return Error::from_string_literal("Expected stream server case");
    TRY(parent(arguments.strings[1]));
    return 0;
}
