/** Exercise the real native transport, without a browser, JS engine or visible window. */
#include "SpawnBytePeer.h"
#include "native/ipc/bytes/Channel.h"
#include <AK/ByteBuffer.h>
#include <AK/ScopeGuard.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/EventLoop.h>
#include <LibCore/System.h>
#include <LibIPC/StartupTransport.h>
#include <LibIPC/TransportHandle.h>
#ifdef AK_OS_WINDOWS
#include <LibIPC/HandleBroker.h>
#endif
#include <LibMain/Main.h>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <string>
#ifdef AK_OS_WINDOWS
#include <io.h>
#endif
#include <thread>

using Firefly::Bytes::Channel;
namespace ByteWire = Firefly::Bytes;

namespace {
ErrorOr<void> until(Core::EventLoop& loop, Function<bool()> done)
{
    Core::ElapsedTimer timer;
    timer.start();
    while (!done()) {
        loop.pump(Core::EventLoop::WaitMode::PollForEvents);
        if (timer.elapsed_milliseconds() > 5000)
            return Error::from_string_literal("Native byte-channel acceptance timeout");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return {};
}

ErrorOr<void> check_packets()
{
    auto payload = TRY(ByteBuffer::create_uninitialized(ByteWire::max_payload));
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<u8>((i * 37 + 11) & 255);
    auto encoded = TRY(ByteWire::encode_packet(ByteWire::PacketKind::Data, 17, payload));
    auto decoded = TRY(ByteWire::decode_packet(encoded.data().span()));
    if (decoded.sequence != 17 || decoded.kind != ByteWire::PacketKind::Data
        || decoded.payload != payload.bytes())
        return Error::from_string_literal("Native packet arbitrary-byte boundary failed");
    for (size_t length : { 0uz, 4uz, 19uz, encoded.data().size() - 1 }) {
        if (!ByteWire::decode_packet(encoded.data().span().slice(0, length)).is_error())
            return Error::from_string_literal("Truncated native packet admitted");
    }

    for (size_t offset : { 0uz, 4uz, 5uz, 6uz, 7uz, 16uz }) {
        auto corrupt = TRY(ByteBuffer::copy(encoded.data().span()));
        corrupt[offset] = 255;
        if (!ByteWire::decode_packet(corrupt).is_error())
            return Error::from_string_literal("Malformed native header admitted");
    }

    auto large = TRY(ByteBuffer::create_zeroed(ByteWire::max_payload + 1));
    if (!ByteWire::encode_packet(ByteWire::PacketKind::Data, 1, large).is_error()
        || !ByteWire::encode_packet(ByteWire::PacketKind::Data, 0, {}).is_error()
        || !ByteWire::encode_packet(ByteWire::PacketKind::Ack, 1, payload).is_error())
        return Error::from_string_literal("Native packet policy bypass");
    outln("PASS native packet raw bytes, exact maximum, malformed headers and length refusal");
    return {};
}

ErrorOr<void> check_channel(Core::EventLoop& loop)
{
    auto pair = TRY(IPC::Transport::create_paired());
    size_t received = 0, writable = 0;
    bool bad = false;
    auto first = TRY(Channel::create(move(pair.local),
        {
            .on_data = [&](ReadonlyBytes) { bad = true; },
            .on_writable = [&] { ++writable; },
            .on_closed = {},
        }));
    auto second = TRY(Channel::create(TRY(pair.remote_handle.create_transport()),
        {
            .on_data =
                [&](ReadonlyBytes data) {
                    if (data.size() != 1 || data[0] != received)
                        bad = true;
                    ++received;
                },
            .on_writable = {},
            .on_closed = {},
        }));
    for (u8 i = 0; i < ByteWire::max_pending; ++i)
        TRY(first->send({ &i, 1 }));
    if (!first->send({}).is_error() || first->pending_send_count() != ByteWire::max_pending
        || received)
        return Error::from_string_literal("Native send credit/deferred delivery failed");
    TRY(until(loop, [&] {
        return received == ByteWire::max_pending && first->pending_send_count() == 0 && writable;
    }));
    if (bad || second->queued_receive_bytes())
        return Error::from_string_literal("Native payload ordering/queue release failed");
    // Capacity returns only after real delivery/ack, not merely a local send.
    auto byte = static_cast<u8>(received);
    TRY(first->send({ &byte, 1 }));
    TRY(until(loop,
        [&] { return received == ByteWire::max_pending + 1 && !first->pending_send_count(); }));
    size_t closed = 0;
    first->close();
    if (!first->send({}).is_error())
        return Error::from_string_literal("Closed channel accepted a send");
    // A separate peer validates kernel EOF without treating a close as an ACK.
    auto loss_pair = TRY(IPC::Transport::create_paired());
    auto lost = TRY(Channel::create(move(loss_pair.local),
        {
            .on_data = [&](ReadonlyBytes) { bad = true; },
            .on_writable = {},
            .on_closed =
                [&](auto reason) {
                    if (reason == Channel::CloseReason::Peer)
                        ++closed;
                    else
                        bad = true;
                },
        }));
    auto peer = TRY(loss_pair.remote_handle.create_transport());
    peer->close();
    TRY(until(loop, [&] { return closed == 1; }));
    if (bad || lost->is_open() || lost->pending_send_count())
        return Error::from_string_literal("Native EOF did not revoke channel");
    outln("PASS actual native channel order, bounded send credit, delivery ACK, reusable capacity "
          "and peer EOF");
    return {};
}

ErrorOr<void> check_hostile(Core::EventLoop& loop)
{
    for (bool replay : { false, true }) {
        auto pair = TRY(IPC::Transport::create_paired());
        size_t received = 0;
        bool rejected = false;
        auto channel = TRY(Channel::create(move(pair.local),
            {
                .on_data = [&](ReadonlyBytes) { ++received; },
                .on_writable = {},
                .on_closed
                = [&](auto reason) { rejected = reason == Channel::CloseReason::Protocol; },
            }));
        auto peer = TRY(pair.remote_handle.create_transport());
        auto packet = TRY(ByteWire::encode_packet(
            replay ? ByteWire::PacketKind::Data : ByteWire::PacketKind::Ack, 1, {}));
        TRY(packet.transfer_message(*peer));
        if (replay) {
            auto duplicate = TRY(ByteWire::encode_packet(ByteWire::PacketKind::Data, 1, {}));
            TRY(duplicate.transfer_message(*peer));
        }

        peer->flush();
        TRY(until(loop, [&] { return rejected; }));
        if (channel->is_open() || received > (replay ? 1uz : 0uz))
            return Error::from_string_literal("Native replay/unsolicited ACK was delivered");
    }

    outln("PASS native unsolicited ACK and replay fail closed");
    return {};
}

ErrorOr<void> check_queue_limit(Core::EventLoop& loop)
{
    auto pair = TRY(IPC::Transport::create_paired());
    auto peer = TRY(pair.remote_handle.create_transport());
    for (u64 i = 1; i <= ByteWire::max_pending + 1; ++i) {
        auto packet = TRY(ByteWire::encode_packet(ByteWire::PacketKind::Data, i, {}));
        TRY(packet.transfer_message(*peer));
    }

    peer->flush();
    pair.local->wait_until_incoming_is_current();
    bool rejected = false;
    size_t received = 0;
    auto channel = TRY(Channel::create(move(pair.local),
        {
            .on_data = [&](ReadonlyBytes) { ++received; },
            .on_writable = {},
            .on_closed = [&](auto reason) { rejected = reason == Channel::CloseReason::Protocol; },
        }));
    TRY(until(loop, [&] { return rejected; }));
    if (received || channel->is_open() || channel->queued_receive_bytes())
        return Error::from_string_literal("Zero-byte flood escaped native count budget");
    outln("PASS native receive count bound includes zero-byte frames and cancels queued delivery");
    return {};
}

ErrorOr<void> check_reentry(Core::EventLoop& loop)
{
    auto pair = TRY(IPC::Transport::create_paired());
    size_t received = 0;
    bool extra = false, nested = false, failed = false;
    RefPtr<Channel> sender, receiver;
    sender = TRY(Channel::create(move(pair.local),
        {
            .on_data = [&](ReadonlyBytes) { failed = true; },
            .on_writable =
                [&] {
                    if (extra)
                        return;
                    u8 value = ByteWire::max_pending;
                    if (sender->send({ &value, 1 }).is_error())
                        failed = true;
                    extra = true;
                },
            .on_closed = {},
        }));
    receiver = TRY(Channel::create(TRY(pair.remote_handle.create_transport()),
        {
            .on_data =
                [&](ReadonlyBytes bytes) {
                    if (nested || bytes.size() != 1 || bytes[0] != received)
                        failed = true;
                    auto index = received++;
                    nested = true;
                    ScopeGuard reset([&] { nested = false; });
                    if (index == 1) {
                        // Admit a new frame while an older dispatch batch is active.
                        // It must neither overtake that batch nor bypass its budget.
                        if (until(loop, [&] {
                                return failed
                                    || (extra
                                        && receiver->queued_receive_bytes()
                                            == ByteWire::max_pending);
                            }).is_error())
                            failed = true;
                    }
                },
            .on_writable = {},
            .on_closed = {},
        }));
    for (u8 i = 0; i < ByteWire::max_pending; ++i)
        TRY(sender->send({ &i, 1 }));
    TRY(until(loop, [&] {
        return failed || (received == ByteWire::max_pending + 1 && !sender->pending_send_count());
    }));
    sender->close();
    receiver->close();
    if (failed)
        return Error::from_string_literal("Native byte dispatch reentry changed order or budget");
    outln("PASS native dispatch reentry retains FIFO and active-batch receive accounting");
    return {};
}

ErrorOr<void> check_large_and_attachment(Core::EventLoop& loop)
{
    auto pair = TRY(IPC::Transport::create_paired());
    bool received = false, bad = false;
    auto sender = TRY(Channel::create(move(pair.local),
        { .on_data = [&](ReadonlyBytes) { bad = true; }, .on_writable = {}, .on_closed = {} }));
    auto receiver = TRY(Channel::create(TRY(pair.remote_handle.create_transport()),
        {
            .on_data =
                [&](ReadonlyBytes bytes) {
                    if (bytes.size() != ByteWire::max_payload)
                        bad = true;
                    for (size_t i = 0; i < bytes.size(); ++i)
                        if (bytes[i] != static_cast<u8>((i * 37 + 11) & 255))
                            bad = true;
                    received = true;
                },
            .on_writable = {},
            .on_closed = {},
        }));
    auto payload = TRY(ByteBuffer::create_uninitialized(ByteWire::max_payload));
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<u8>((i * 37 + 11) & 255);
    TRY(sender->send(payload));
    __builtin_memset(payload.data(), 0, payload.size());
    TRY(until(loop, [&] { return received && !sender->pending_send_count(); }));
    if (bad)
        return Error::from_string_literal("Maximum native payload/source ownership failed");
    sender->close();
    receiver->close();
    auto hostile = TRY(IPC::Transport::create_paired());
    bool rejected = false;
    auto channel = TRY(Channel::create(move(hostile.local),
        {
            .on_data = [&](ReadonlyBytes) { bad = true; },
            .on_writable = {},
            .on_closed = [&](auto reason) { rejected = reason == Channel::CloseReason::Protocol; },
        }));
    auto peer = TRY(hostile.remote_handle.create_transport());
#ifdef AK_OS_WINDOWS
    auto descriptor = TRY(Core::System::open("NUL"sv, O_RDONLY));
#else
    auto descriptor = TRY(Core::System::open("/dev/null"sv, O_RDONLY | O_CLOEXEC));
#endif
    auto packet = TRY(ByteWire::encode_packet(ByteWire::PacketKind::Data, 1, {}));
    TRY(packet.append_attachment(IPC::Attachment::from_fd(descriptor)));
    TRY(packet.transfer_message(*peer));
    TRY(until(loop, [&] { return rejected; }));
    if (bad || channel->is_open())
        return Error::from_string_literal("Unexpected native handle admitted");
    outln("PASS maximum native payload/source mutation and unexpected handle refusal");
    return {};
}
}

ErrorOr<int> run_byte_child(Core::EventLoop&, StringView);
ErrorOr<void> check_byte_process(Core::EventLoop&, ByteString const&);

ErrorOr<int> ladybird_main(Main::Arguments args)
{
#ifdef AK_OS_WINDOWS
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1)
        return Error::from_string_literal("Cannot establish native fixture binary streams");
#endif
    auto& loop = Core::EventLoop::initialize_for_current_thread();
    if (args.argc == 2 && args.strings[1] == "packet-encode"sv) {
        auto payload = TRY(ByteBuffer::create_uninitialized(ByteWire::max_payload));
        for (size_t i = 0; i < payload.size(); ++i)
            payload[i] = static_cast<u8>((i * 37 + 11) & 255);
        auto packet = TRY(ByteWire::encode_packet(ByteWire::PacketKind::Data, 17, payload));
        if (fwrite(packet.data().data(), 1, packet.data().size(), stdout) != packet.data().size())
            return Error::from_string_literal("Cannot write native packet fixture");
        return 0;
    }

    if (args.argc == 2 && args.strings[1] == "packet-decode"sv) {
        std::string input(ByteWire::header_size + ByteWire::max_payload + 1, '\0');
        std::cin.read(input.data(), static_cast<std::streamsize>(input.size()));
        input.resize(static_cast<size_t>(std::cin.gcount()));
        auto packet = TRY(
            ByteWire::decode_packet({ reinterpret_cast<u8 const*>(input.data()), input.size() }));
        if (packet.kind != ByteWire::PacketKind::Data || packet.sequence != 17
            || packet.payload.size() != ByteWire::max_payload)
            return Error::from_string_literal("Independent native packet fixture mismatch");
        for (size_t i = 0; i < packet.payload.size(); ++i)
            if (packet.payload[i] != static_cast<u8>((i * 37 + 11) & 255))
                return Error::from_string_literal("Independent native packet bytes mismatch");
        outln("PASS independent native packet");
        return 0;
    }

    if (args.argc == 3 && args.strings[1] == "child"sv) {
        // An abruptly killed test parent must not leave a child blocked in
        // synchronous bootstrap. This watchdog belongs only to the fixture.
        std::mutex lock;
        std::condition_variable wake;
        bool done = false;
        std::thread watchdog([&] {
            std::unique_lock guard(lock);
            if (!wake.wait_for(guard, std::chrono::seconds(15), [&] { return done; }))
                Core::Process::terminate_immediately(1);
        });
        ScopeGuard stop_watchdog([&] {
            {
                std::lock_guard guard(lock);
                done = true;
            }

            wake.notify_one();
            watchdog.join();
        });
        return run_byte_child(loop, args.strings[2]);
    }
#ifdef AK_OS_WINDOWS
    IPC::HandleBroker::install(make<IPC::LocalHandleBroker>());
#endif
    if (args.argc == 2 && args.strings[1] == "process"sv) {
        TRY(check_byte_process(loop, ByteString(args.strings[0])));
        return 0;
    }

    if (args.argc != 1 && !(args.argc == 2 && args.strings[1] == "checks"sv))
        return Error::from_string_literal("Expected checks or process");
    TRY(check_packets());
    TRY(check_channel(loop));
    TRY(check_hostile(loop));
    TRY(check_queue_limit(loop));
    TRY(check_reentry(loop));
    TRY(check_large_and_attachment(loop));
    return 0;
}

ErrorOr<int> run_byte_child(Core::EventLoop& loop, StringView server)
{
    size_t received = 0;
    bool failed = false, closed = false;
    RefPtr<Channel> channel;
    channel = TRY(Channel::create(TRY(IPC::take_over_startup_transport(server)),
        {
            .on_data =
                [&](ReadonlyBytes bytes) {
                    if ((received == 0 && bytes.size() != 65536)
                        || (received == 1 && !bytes.is_empty()) || received > 1)
                        failed = true;
                    for (size_t i = 0; i < bytes.size(); ++i)
                        if (bytes[i] != static_cast<u8>((i * 37 + 11) & 255))
                            failed = true;
                    auto reply = MUST(ByteBuffer::create_uninitialized(8 + bytes.size()));
                    auto pid = static_cast<u64>(Core::System::getpid());
                    for (size_t i = 0; i < 8; ++i)
                        reply[i] = static_cast<u8>(pid >> (i * 8));
                    if (!bytes.is_empty())
                        __builtin_memcpy(reply.data() + 8, bytes.data(), bytes.size());
                    if (channel->send(reply).is_error())
                        failed = true;
                    ++received;
                },
            .on_writable = {},
            .on_closed =
                [&](auto) {
                    closed = true;
                    if (received != 2)
                        failed = true;
                },
        }));
    TRY(until(loop, [&] { return failed || (received == 2 && !channel->pending_send_count()); }));
    if (failed)
        return Error::from_string_literal("Native byte peer failed");
    channel->close();
    TRY(until(loop, [&] { return closed; }));
    return 0;
}

ErrorOr<void> check_byte_process(Core::EventLoop& loop, ByteString const& executable)
{
    auto peer = TRY(spawn_byte_peer(executable));
    if (peer->process.pid() == Core::System::getpid())
        return Error::from_string_literal("Native byte peer did not get a separate PID");
    size_t received = 0;
    bool failed = false, closed = false;
    auto channel = TRY(Channel::create(move(peer->transport),
        {
            .on_data =
                [&](ReadonlyBytes bytes) {
                    if (bytes.size() != (received == 0 ? 65544uz : 8uz) || received > 1) {
                        failed = true;
                        return;
                    }

                    u64 pid = 0;
                    for (size_t i = 0; i < 8; ++i)
                        pid |= static_cast<u64>(bytes[i]) << (i * 8);
                    if (pid != static_cast<u64>(peer->process.pid()))
                        failed = true;
                    for (size_t i = 8; i < bytes.size(); ++i)
                        if (bytes[i] != static_cast<u8>(((i - 8) * 37 + 11) & 255))
                            failed = true;
                    ++received;
                },
            .on_writable = {},
            .on_closed =
                [&](auto) {
                    closed = true;
                    if (received != 2)
                        failed = true;
                },
        }));
    auto payload = TRY(ByteBuffer::create_uninitialized(65536));
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<u8>((i * 37 + 11) & 255);
    TRY(channel->send(payload));
    TRY(until(loop, [&] { return failed || (received == 1 && !channel->pending_send_count()); }));
    if (failed)
        return Error::from_string_literal("Native byte parent data assertion failed");
    TRY(channel->send({}));
    TRY(until(loop, [&] { return failed || (received == 2 && !channel->pending_send_count()); }));
    if (failed || TRY(peer->wait()) != 0)
        return Error::from_string_literal("Native byte peer did not complete successfully");
    channel->close();
    TRY(until(loop, [&] { return closed; }));
    outln("PASS actual native child byte round trip, OS PID identity, arbitrary bytes and empty "
          "message");
    outln("{{\"parent\":{},\"child\":{},\"bytes\":65536}}", Core::System::getpid(),
        peer->process.pid());
    return {};
}
