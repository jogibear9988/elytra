/** Real raw codec/retention boundaries, without a JS engine, browser or GUI. */
#include "native/ipc/application/Fragment.h"
#include "native/ipc/application/WireBudget.h"
#include <AK/Array.h>
#include <AK/ByteBuffer.h>
#include <AK/Format.h>
#include <AK/Vector.h>
#include <LibMain/Main.h>

namespace A = Firefly::ApplicationIpc;

namespace {
constexpr u64 epoch = 0x1122334455667788ULL;
constexpr u32 request = 0xaabbccdd;

ErrorOr<void> require(bool value, StringView message)
{
    if (!value)
        return Error::from_string_view(message);
    return {};
}

void write32(Bytes bytes, size_t position, u32 value)
{
    for (size_t i = 0; i < 4; ++i)
        bytes[position + i] = static_cast<u8>(value >> (8 * i));
}

ErrorOr<void> roundtrip(size_t size)
{
    auto wire = TRY(ByteBuffer::create_uninitialized(size));
    for (size_t i = 0; i < size; ++i)
        wire[i] = static_cast<u8>(i * 73 + 255);
    A::Reassembler receiver;
    size_t offset = 0, frames = 0;
    do {
        auto frame = TRY(A::FragmentEncoder::encode_next(
            A::Kind::Reply, true, epoch, request, wire.bytes(), offset));
        if (size <= A::wire_limit) {
            auto legacy
                = TRY(A::encode_message(A::Kind::Reply, true, epoch, request, wire.bytes()));
            TRY(require(legacy == frame.bytes, "Small FIM1 bytes changed"sv));
        }

        TRY(require(frame.bytes.length() <= 128 * 1024 && frame.next_offset > offset,
            "Physical frame bound or sender forward progress failed"sv));
        TRY(require(TRY(A::Reassembler::required_bytes(frame.bytes.bytes(), epoch)) == size,
            "Logical preallocation reservation changed"sv));
        auto decoded = TRY(receiver.accept(frame.bytes.bytes(), epoch));
        offset = frame.next_offset;
        ++frames;
        if (frame.complete) {
            TRY(require(decoded.has_value() && !receiver.active() && receiver.retained_bytes() == 0
                    && decoded->payload.bytes() == wire.bytes() && decoded->kind == A::Kind::Reply
                    && decoded->success && decoded->epoch == epoch && decoded->request == request,
                "Completed logical application identity/bytes changed"sv));
        } else {
            TRY(require(!decoded.has_value() && receiver.active() && receiver.byte_count() == size,
                "Partial application payload escaped or reservation undercounted"sv));
        }
    } while (offset < wire.size());
    if (size <= A::wire_limit)
        TRY(require(frames == 1, "Legacy FIM1 was fragmented"sv));
    if (size == A::logical_wire_limit)
        TRY(require(frames > A::queue_limit && frames > 16,
            "Maximum logical stream did not exercise more than queue/native credits"sv));
    return {};
}

ErrorOr<void> malformed()
{
    auto wire = TRY(ByteBuffer::create_zeroed(A::wire_limit + 1));
    auto first = TRY(
        A::FragmentEncoder::encode_next(A::Kind::Reply, true, epoch, request, wire.bytes(), 0));
    Array<u8, 32> independent { 'F', 'I', 'M', '2', 3, 1, 0, 0, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33,
        0x22, 0x11, 0xdd, 0xcc, 0xbb, 0xaa, 0xed, 0xff, 1, 0, 0, 0, 0, 0, 0xe0, 0xff, 1, 0 };
    TRY(require(first.bytes.bytes().slice(0, 32) == independent.span(),
        "FIM2 fixed independent little-endian header changed"sv));
    auto last = TRY(A::FragmentEncoder::encode_next(
        A::Kind::Reply, true, epoch, request, wire.bytes(), first.next_offset));
    for (size_t length : { 0uz, 3uz, 19uz, 31uz, first.bytes.length() - 1 })
        TRY(require(
            A::Reassembler::required_bytes(first.bytes.bytes().slice(0, length), epoch).is_error(),
            "Truncated fragment admitted before allocation"sv));
    TRY(require(A::Reassembler::required_bytes(first.bytes.bytes(), epoch + 1).is_error(),
        "Stale fragment epoch admitted"sv));
    for (size_t position : { 0uz, 4uz, 5uz, 6uz, 7uz }) {
        auto changed = TRY(ByteBuffer::copy(first.bytes.bytes()));
        changed[position] = position == 4 ? 5 : position == 5 ? 2 : changed[position] ^ 1;
        TRY(require(A::Reassembler::required_bytes(changed.bytes(), epoch).is_error(),
            "Invalid fragment tag/flags admitted"sv));
    }

    struct Field {
        size_t position;
        u32 value;
    };

    Array<Field, 5> invalid_fields { Field { 20, A::logical_wire_limit + 1 }, { 20, A::wire_limit },
        { 24, static_cast<u32>(wire.size()) }, { 28, 0 }, { 28, A::fragment_payload_limit + 1 } };
    for (auto [position, value] : invalid_fields) {
        auto changed = TRY(ByteBuffer::copy(first.bytes.bytes()));
        write32(changed.bytes(), position, value);
        A::Reassembler receiver;
        TRY(require(A::Reassembler::required_bytes(changed.bytes(), epoch).is_error()
                && receiver.accept(changed.bytes(), epoch).is_error() && !receiver.active(),
            "Invalid/over-budget fragment allocated or escaped"sv));
    }

    for (size_t position : { 4uz, 5uz, 8uz, 16uz, 20uz, 24uz }) {
        A::Reassembler receiver;
        TRY(require(!TRY(receiver.accept(first.bytes.bytes(), epoch)).has_value(),
            "First fragment unexpectedly completed"sv));
        auto changed = TRY(ByteBuffer::copy(last.bytes.bytes()));
        changed[position] ^= 1;
        TRY(require(receiver.accept(changed.bytes(), epoch).is_error(),
            "Changed fragment identity/total/overlap admitted"sv));
        receiver.reset();
        TRY(require(!receiver.active() && receiver.byte_count() == 0,
            "Cancellation/reset retained partial bytes"sv));
    }

    for (bool total_changed : { false, true }) {
        A::Reassembler receiver;
        TRY(require(!TRY(receiver.accept(first.bytes.bytes(), epoch)).has_value(),
            "First fragment unexpectedly completed"sv));
        auto changed = TRY(ByteBuffer::copy(last.bytes.bytes()));
        write32(changed.bytes(), total_changed ? 20 : 24,
            total_changed ? static_cast<u32>(wire.size() + 1)
                          : static_cast<u32>(first.next_offset - 1));
        TRY(A::Reassembler::required_bytes(changed.bytes(), epoch));
        TRY(require(receiver.accept(changed.bytes(), epoch).is_error(),
            "Well-formed conflicting total/overlap admitted"sv));
    }

    A::Reassembler receiver;
    TRY(require(receiver.accept(last.bytes.bytes(), epoch).is_error(),
        "Fragment gap/start at nonzero offset admitted"sv));
    TRY(require(!TRY(receiver.accept(first.bytes.bytes(), epoch)).has_value(),
        "First fragment unexpectedly completed"sv));
    TRY(require(receiver.accept(first.bytes.bytes(), epoch).is_error(),
        "Duplicate/restarted fragment admitted"sv));
    auto ack = TRY(A::encode_message(A::Kind::Ack, false, epoch, request, {}));
    TRY(require(
        receiver.accept(ack.bytes(), epoch).is_error(), "Interleaved FIM1 frame admitted"sv));
    A::Reassembler moved = move(receiver);
    TRY(require(
        !receiver.active() && moved.active(), "Moved partial assembly retained two owners"sv));
    auto complete = TRY(moved.accept(last.bytes.bytes(), epoch));
    TRY(require(complete.has_value() && complete->payload.bytes() == wire.bytes(),
        "Moved partial assembly lost ownership"sv));
    auto ack_message = TRY(receiver.accept(ack.bytes(), epoch));
    TRY(require(ack_message.has_value() && ack_message->kind == A::Kind::Ack
            && ack_message->payload.is_empty(),
        "Legacy empty Ack changed"sv));
    auto invalid = TRY(ByteBuffer::create_zeroed(A::logical_wire_limit + 1));
    TRY(require(
        A::FragmentEncoder::encode_next(A::Kind::Send, false, epoch, request, invalid.bytes(), 0)
                .is_error()
            && A::FragmentEncoder::encode_next(A::Kind::Send, true, epoch, request, wire.bytes(), 0)
                .is_error()
            && A::FragmentEncoder::encode_next(A::Kind::Ack, false, epoch, request, wire.bytes(), 0)
                .is_error()
            && A::FragmentEncoder::encode_next(
                A::Kind::Send, false, epoch, request, wire.bytes(), wire.size())
                .is_error(),
        "Invalid fragmented encoder fields admitted"sv));
    return {};
}

ErrorOr<void> quotas()
{
    Vector<NonnullRefPtr<Firefly::IpcWireBudget>> documents;
    Vector<RefPtr<Firefly::IpcWireReservation>> reservations;
    for (size_t i = 0; i < 5; ++i)
        documents.append(adopt_ref(*new Firefly::IpcWireBudget));
    for (size_t i = 0; i < 4; ++i) {
        auto lease = documents[i]->reserve(A::logical_wire_limit);
        TRY(require(
            !!lease && !documents[i]->reserve(1), "Per-document reservation exceeded 16 MiB"sv));
        reservations.append(move(lease));
    }

    TRY(require(!documents[4]->reserve(1), "Process reservation exceeded 64 MiB"sv));
    auto retained = reservations[0];
    reservations[0] = nullptr;
    TRY(require(!documents[4]->reserve(1), "Queued owner copy lost its native reservation"sv));
    retained = nullptr;
    auto replacement = documents[4]->reserve(A::logical_wire_limit);
    TRY(require(!!replacement, "Released native reservation did not permit admission"sv));
    return {};
}
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    for (size_t size : { 1uz, A::wire_limit - 1, A::wire_limit, A::wire_limit + 1,
             6uz * 1024 * 1024 + 17, A::logical_wire_limit })
        TRY(roundtrip(size));
    TRY(malformed());
    TRY(quotas());
    outln("PASS application FIM1/FIM2 real native full streams through 16 MiB, strict malformed "
          "refusal, reset/move ownership and document/process RAII quotas");
    return 0;
}
