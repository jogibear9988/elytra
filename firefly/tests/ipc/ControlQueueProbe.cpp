/** Shared production scheduling under real byte-channel backpressure; no VM, GUI or fake ACKs. */
#include "native/ipc/application/Fragment.h"
#include "native/ipc/application/OutgoingQueue.h"
#include "native/ipc/application/WireBudget.h"
#include "native/ipc/bytes/Channel.h"
#include "native/utility/ports/Fragments.h"
#include <AK/ByteBuffer.h>
#include <AK/Format.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/EventLoop.h>
#include <LibIPC/StartupTransport.h>
#include <LibMain/Main.h>
#ifdef AK_OS_WINDOWS
#include <LibIPC/HandleBroker.h>
#endif
#include <chrono>
#include <thread>

namespace A = Firefly::ApplicationIpc;
namespace B = Firefly::Bytes;
namespace P = Firefly::Utility::Ports;

namespace {
constexpr u64 epoch = 17;

struct Packet {
    A::Kind kind;
    u32 request;
    ByteString wire;
    size_t offset { 0 };
    RefPtr<Firefly::IpcWireReservation> lease {};
};

using Queue = A::OutgoingQueue<Packet>;

ErrorOr<void> require(bool value, StringView message)
{
    if (!value)
        return Error::from_string_view(message);
    return {};
}

Packet packet(u32 request, A::Kind kind = A::Kind::PortControl)
{
    auto text = ByteString::formatted("owned-{}", request);
    if (kind != A::Kind::PortControl)
        return { kind, request, move(text) };
    auto encoded = P::encode(P::Kind::Data, { 1, 1 }, {}, { text.characters(), text.length() });
    return { kind, request, ByteString(encoded.data(), encoded.size()) };
}

ErrorOr<void> capacity()
{
    Queue queue;
    for (u32 i = 0; i < 16; ++i)
        queue.append(packet(i + 1), A::Traffic::Bulk);
    TRY(require(!queue.can_append(A::Traffic::Bulk) && queue.size() == 16,
        "Bulk consumed the sequenced-control reserve"sv));
    for (u32 i = 0; i < 16; ++i)
        queue.append(packet(i + 17), A::Traffic::SequencedControl);
    TRY(require(!queue.can_append(A::Traffic::SequencedControl) && queue.size() == 32,
        "Sequenced traffic consumed the critical reserve"sv));
    for (u32 i = 0; i < 16; ++i)
        queue.append(packet(i + 33, A::Kind::Reply), A::Traffic::Critical);
    TRY(require(!queue.can_append(A::Traffic::Critical) && queue.size() == 48,
        "Critical queue exceeded the existing physical ceiling"sv));
    queue.append(packet(100, A::Kind::Event), A::Traffic::Event);
    TRY(require(queue.size() == 49 && !queue.can_append(A::Traffic::Event)
            && !queue.can_append(A::Traffic::Critical),
        "Existing single event slot was enlarged or consumed controls"sv));
    queue.clear();

    auto fragments = P::fragment_count(P::logical_limit);
    TRY(require(fragments == 9, "Maximum logical port DATA geometry changed"sv));
    for (u32 i = 0; i < 8; ++i)
        queue.append(packet(i + 1), A::Traffic::Bulk);
    auto before = queue.size();
    TRY(require(!queue.can_append(A::Traffic::Bulk, fragments) && queue.size() == before,
        "Whole logical DATA preflight partially changed admission"sv));
    TRY(require(!queue.can_append(A::Traffic::Bulk, static_cast<size_t>(-1)),
        "Overflowing fragment count bypassed preflight"sv));
    queue.clear();
    for (u32 i = 0; i < 7; ++i)
        queue.append(packet(i + 1), A::Traffic::Bulk);
    TRY(require(
        queue.can_append(A::Traffic::Bulk, fragments), "Valid complete fragment group refused"sv));
    auto payload = TRY(ByteBuffer::create_zeroed(P::logical_limit));
    size_t offset = 0;
    for (size_t i = 0; i < fragments; ++i) {
        auto frame = P::encode_next(
            { 1, 1 }, 1, { reinterpret_cast<char const*>(payload.data()), payload.size() }, offset);
        TRY(require(!frame.bytes.empty() && frame.next_offset > offset,
            "Genuine port fragment encoder failed"sv));
        queue.append({ A::Kind::PortControl, static_cast<u32>(i + 8),
                         ByteString(frame.bytes.data(), frame.bytes.size()) },
            A::Traffic::Bulk);
        offset = frame.next_offset;
    }

    TRY(require(
        offset == payload.size() && queue.size() == 16 && !queue.can_append(A::Traffic::Bulk),
        "Complete fragment reservation changed the bulk ceiling"sv));
    return {};
}

ErrorOr<void> ordering()
{
    Queue queue;
    queue.append(packet(1), A::Traffic::Bulk);
    for (u32 i = 0; i < 16; ++i)
        queue.append(packet(i + 2, A::Kind::Reply), A::Traffic::Critical);
    for (u32 i = 0; i < 16; ++i) {
        TRY(require(queue.first().request == i + 2, "Critical FIFO was reordered"sv));
        queue.take_first();
        queue.append(packet(i + 100, A::Kind::Reply), A::Traffic::Critical);
    }

    TRY(require(queue.first().request == 1, "Continued critical arrivals starved oldest bulk"sv));
    queue.clear();

    // Actual FIM2 offsets, not a fabricated started bit: an intervening reply
    // cannot corrupt the peer's one contiguous logical-message reassembler.
    auto payload = TRY(ByteBuffer::create_zeroed(A::wire_limit + 23));
    queue.append({ A::Kind::Invoke, 1, ByteString(payload.bytes()) }, A::Traffic::Bulk);
    auto first = TRY(A::FragmentEncoder::encode_next(
        A::Kind::Invoke, false, epoch, 1, queue.first().wire.bytes(), 0));
    TRY(require(!first.complete, "Partial-message fixture was not fragmented"sv));
    queue.first().offset = first.next_offset;
    queue.append(packet(2, A::Kind::Reply), A::Traffic::Critical);
    TRY(require(
        queue.first().request == 1, "Critical packet interrupted an emitted FIM2 message"sv));
    A::Reassembler receiver;
    TRY(require(!TRY(receiver.accept(first.bytes.bytes(), epoch)).has_value(),
        "Partial FIM2 was published"sv));
    auto final = TRY(A::FragmentEncoder::encode_next(
        A::Kind::Invoke, false, epoch, 1, queue.first().wire.bytes(), queue.first().offset));
    auto complete = TRY(receiver.accept(final.bytes.bytes(), epoch));
    TRY(require(
        final.complete && complete.has_value() && complete->payload.bytes() == payload.bytes(),
        "Pinned logical message did not complete byte-exactly"sv));
    queue.take_first();
    TRY(require(
        queue.first().request == 2, "Critical reply did not progress after partial completion"sv));
    return {};
}

ErrorOr<void> pressure(Core::EventLoop& loop)
{
    auto pair = TRY(IPC::Transport::create_paired());
    Queue queue;
    auto budget = adopt_ref(*new Firefly::IpcWireBudget);
    Vector<u32> received;
    bool failed = false;
    RefPtr<B::Channel> sender;
    auto flush = [&] {
        while (!queue.is_empty() && sender->pending_send_count() < B::max_pending) {
            auto& current = queue.first();
            auto frame = A::FragmentEncoder::encode_next(
                current.kind, false, epoch, current.request, current.wire.bytes(), current.offset);
            if (frame.is_error() || sender->send(frame.value().bytes.bytes()).is_error()) {
                failed = true;
                return;
            }

            current.offset = frame.value().next_offset;
            if (frame.value().complete)
                queue.take_first();
        }
    };

    sender = TRY(B::Channel::create(move(pair.local),
        {
            .on_data = [&](ReadonlyBytes) { failed = true; },
            .on_writable = [&] { flush(); },
            .on_closed = {},
        }));
    // The real remote transport exists, but no receiver channel is pumping or
    // manufacturing ACKs. These are the transport's genuine 16 outstanding sends.
    for (u32 i = 1; i <= 16; ++i) {
        auto initial = packet(i);
        auto message
            = TRY(A::encode_message(A::Kind::PortControl, false, epoch, i, initial.wire.bytes()));
        TRY(sender->send(message.bytes()));
    }

    TRY(require(sender->pending_send_count() == 16, "Real byte send window did not saturate"sv));
    for (u32 i = 17; i <= 32; ++i) {
        auto value = packet(i);
        value.lease = budget->reserve(value.wire.length());
        TRY(require(!!value.lease, "Queue wire reservation failed"sv));
        queue.append(move(value), A::Traffic::Bulk);
    }

    TRY(require(!queue.can_append(A::Traffic::Bulk), "Bulk did not backpressure at 16"sv));
    auto size = P::size_payload(1);
    auto ack = P::encode(P::Kind::Ack, { 1, 1 }, {}, size);
    auto close = P::encode(P::Kind::Close, { 1, 1 }, {}, {});
    queue.append({ A::Kind::PortControl, 33, ByteString(ack.data(), ack.size()) },
        A::Traffic::SequencedControl);
    queue.append({ A::Kind::PortControl, 34, ByteString(close.data(), close.size()) },
        A::Traffic::SequencedControl);
    queue.append(packet(500, A::Kind::Reply), A::Traffic::Critical);
    queue.append({ A::Kind::Ack, 501, {} }, A::Traffic::Critical);
    queue.append({ A::Kind::PortAck, 502, {} }, A::Traffic::Critical);
    flush();
    TRY(require(queue.size() == 21 && sender->pending_send_count() == 16,
        "Blocked physical channel lost queued controls or forged credit"sv));
    TRY(require(
        !budget->reserve(A::logical_wire_limit), "Queued payloads lost their native leases"sv));

    auto receiver = TRY(B::Channel::create(TRY(pair.remote_handle.create_transport()),
        {
            .on_data =
                [&](ReadonlyBytes bytes) {
                    auto message = A::decode_message(bytes);
                    if (message.is_error() || message.value().epoch != epoch) {
                        failed = true;
                        return;
                    }

                    if (message.value().kind == A::Kind::PortControl) {
                        P::Packet data;
                        auto bytes = message.value().payload;
                        auto expected = ByteString::formatted("owned-{}", message.value().request);
                        if (!P::decode(
                                { reinterpret_cast<char const*>(bytes.data()), bytes.size() }, data)
                            || (message.value().request <= 32 && data.kind != P::Kind::Data)
                            || (message.value().request == 33 && data.kind != P::Kind::Ack)
                            || (message.value().request == 34 && data.kind != P::Kind::Close)
                            || (message.value().request <= 32
                                && StringView(data.payload.data(), data.payload.size())
                                    != expected.view())) {
                            failed = true;
                            return;
                        }
                    }

                    received.append(message.value().request);
                },
            .on_writable = {},
            .on_closed = {},
        }));
    Core::ElapsedTimer timer;
    timer.start();
    while (
        !failed && (received.size() != 37 || !queue.is_empty() || sender->pending_send_count())) {
        loop.pump(Core::EventLoop::WaitMode::PollForEvents);
        if (timer.elapsed_milliseconds() > 5000)
            return Error::from_string_literal("Real control queue pressure deadline");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    TRY(require(!failed && sender->is_open() && receiver->is_open(),
        "Legitimate data/control pressure retired the actual transport"sv));
    for (u32 i = 0; i < 16; ++i)
        TRY(require(received[i] == i + 1, "Original byte-window FIFO changed"sv));
    TRY(require(received[16] == 500 && received[17] == 501 && received[18] == 502,
        "Critical reply/ACK did not pass unstarted bulk"sv));
    for (u32 i = 0; i < 18; ++i)
        TRY(require(
            received[i + 19] == i + 17, "Sequenced DATA/ACK/CLOSE relative order changed"sv));
    auto refunded = budget->reserve(A::logical_wire_limit);
    TRY(require(!!refunded, "Drained queue leaked wire reservations"sv));
    refunded = nullptr;
    sender->close();
    receiver->close();
    return {};
}

ErrorOr<void> cancellation()
{
    Queue queue;
    auto budget = adopt_ref(*new Firefly::IpcWireBudget);
    auto lease = budget->reserve(A::logical_wire_limit);
    queue.append({ A::Kind::Reply, 1, {}, 0, lease }, A::Traffic::Critical);
    lease = nullptr;
    auto retained = queue.first().lease;
    queue.clear();
    TRY(require(
        !budget->reserve(1), "Canceled packet's retained owner prematurely refunded bytes"sv));
    retained = nullptr;
    TRY(require(
        !!budget->reserve(A::logical_wire_limit), "Cancellation leaked the final native lease"sv));
    TRY(require(queue.is_empty() && queue.can_append(A::Traffic::Bulk, 16),
        "Canceled physical queue did not recover capacity"sv));
    auto payload = TRY(ByteBuffer::create_zeroed(P::logical_limit));
    auto batch = budget->reserve(P::fragment_wire_bytes(payload.size()));
    auto count = P::fragment_count(payload.size());
    TRY(require(
        !!batch && queue.can_append(A::Traffic::Bulk, count), "Whole batch reservation failed"sv));
    size_t offset = 0;
    for (size_t i = 0; i < count; ++i) {
        auto frame = P::encode_next(
            { 1, 1 }, 1, { reinterpret_cast<char const*>(payload.data()), payload.size() }, offset);
        queue.append({ A::Kind::PortControl, static_cast<u32>(i + 1),
                         ByteString(frame.bytes.data(), frame.bytes.size()), 0, batch },
            A::Traffic::Bulk);
        offset = frame.next_offset;
    }

    batch = nullptr;
    for (size_t i = 1; i < count; ++i)
        queue.take_first();
    TRY(require(!budget->reserve(A::logical_wire_limit),
        "Early fragment refunded the shared logical batch"sv));
    queue.clear();
    TRY(require(!!budget->reserve(A::logical_wire_limit),
        "Final batch fragment leaked its shared lease"sv));
    return {};
}
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    Core::EventLoop loop;
#ifdef AK_OS_WINDOWS
    IPC::HandleBroker::install(make<IPC::LocalHandleBroker>());
#endif
    TRY(capacity());
    TRY(ordering());
    TRY(pressure(loop));
    TRY(cancellation());
    outln("PASS production physical queue nested reserves, atomic fragments, fairness, real byte "
          "ACK progress, sequencing and lease cancellation");
    return 0;
}
