/** Pure native protocol/accounting controls; no VM, child, network, or OS handle is fabricated. */
#include "native/utility/ports/Fragments.h"
#include "native/utility/ports/Ledger.h"
#include <cstdio>
#include <cstdlib>

using namespace Firefly::Utility::Ports;

static void require(bool value, char const* label)
{
    if (!value) {
        std::fprintf(stderr, "FAIL utility port protocol: %s\n", label);
        std::exit(1);
    }
}

static Identity admit(Ledger& ledger)
{
    std::vector<Identity> identities;
    require(ledger.prepare_transfer(1, identities) && ledger.admit_transfer(identities),
        "fragment route admission");
    return identities[0];
}

static void field(std::vector<char>& frame, size_t offset, uint32_t value)
{
    auto bytes = size_payload(value);
    for (size_t index = 0; index < bytes.size(); ++index)
        frame[header_size + offset + index] = bytes[index];
}

static void fragments()
{
    auto small_limit = physical_limit - header_size;
    require(fragment_count(0) == 0 && fragment_count(logical_limit + 1) == 0,
        "fragment logical bounds");
    require(fragment_count(small_limit) == 1 && fragment_count(small_limit + 1) == 2
            && fragment_count(logical_limit) == 9,
        "physical fragment count");
    for (auto size : { small_limit, small_limit + 1, size_t { 200000 }, logical_limit }) {
        Ledger receiver;
        auto route = admit(receiver);
        Reassembler assembler;
        std::vector<char> bytes(size);
        for (size_t index = 0; index < size; ++index)
            bytes[index] = static_cast<char>(index * 37 + 11);
        size_t offset = 0, count = 0, physical_bytes = 0;
        do {
            auto frame = encode_next(route, 1, bytes, offset);
            require(!frame.bytes.empty() && frame.bytes.size() <= physical_limit
                    && frame.next_offset > offset,
                "bounded encoder progress");
            Packet packet {};
            require(decode(frame.bytes, packet), "fragment wire decode");
            if (size == small_limit) {
                require(packet.kind == Kind::Data
                        && frame.bytes == encode(Kind::Data, route, {}, bytes),
                    "small DATA byte-identical");
            } else {
                auto result = assembler.accept(packet, receiver);
                require(result.status
                        == (frame.complete ? Reassembler::Status::Complete
                                           : Reassembler::Status::Pending),
                    "single logical completion");
                if (frame.complete)
                    require(result.payload == bytes && !assembler.active(route),
                        "large logical bytes preserved");
                else
                    require(result.payload.empty() && assembler.active(route),
                        "no partial payload publication");
            }

            offset = frame.next_offset;
            ++count;
            physical_bytes += frame.bytes.size();
        } while (offset < bytes.size());
        require(count == fragment_count(size) && physical_bytes == fragment_wire_bytes(size),
            "batch preflight exact frame and byte accounting");
        if (size != small_limit) {
            require(
                receiver.can_consume(route, size), "completion retains one logical receive debit");
            receiver.consumed(route, size);
            auto replay = encode_next(route, 1, bytes, 0);
            Packet packet {};
            require(decode(replay.bytes, packet)
                    && assembler.accept(packet, receiver).status == Reassembler::Status::Invalid,
                "completed serial replay rejected without new debit");
            require(!receiver.can_consume(route, size), "replay does not reserve quota");
            auto newer = encode_next(route, 2, bytes, 0);
            require(decode(newer.bytes, packet)
                    && assembler.accept(packet, receiver).status == Reassembler::Status::Pending,
                "strictly newer serial admitted after completion");
            assembler.close(route);
            receiver.close(route);
        }
    }

    Ledger receiver;
    auto route = admit(receiver);
    Reassembler assembler;
    std::vector<char> bytes(logical_limit, 'q');
    auto first = encode_next(route, 7, bytes, 0);
    Packet packet {};
    for (auto pair :
        { std::pair<size_t, uint32_t> { 0, 0 }, { 4, static_cast<uint32_t>(logical_limit + 1) },
            { 4, static_cast<uint32_t>(small_limit) },
            { 8, static_cast<uint32_t>(logical_limit) } }) {
        auto malformed = first.bytes;
        field(malformed, pair.first, pair.second);
        require(!decode(malformed, packet), "malformed fragment metadata rejected by wire decoder");
    }

    auto short_frame = first.bytes;
    short_frame.resize(header_size + fragment_header_size);
    require(!decode(short_frame, packet), "empty fragment chunk rejected");
    auto oversized = first.bytes;
    oversized.resize(physical_limit + 1);
    require(!decode(oversized, packet), "physical frame budget");
    require(encode_next(route, 0, bytes, 0).bytes.empty()
            && encode_next(route, 1, bytes, 1).bytes.empty()
            && encode_next(route, 1, bytes, bytes.size()).bytes.empty(),
        "invalid encoder cursor/serial");
    auto start = Reassembler::Clock::now();
    require(decode(first.bytes, packet)
            && assembler.accept(packet, receiver, start).status == Reassembler::Status::Pending,
        "first fragment reserves actual logical quota");
    require(!receiver.received(route, 1), "full logical debit before completion");
    auto second = encode_next(route, 7, bytes, first.next_offset);
    auto wrong = second.bytes;
    field(wrong, 8, static_cast<uint32_t>(first.next_offset + 1));
    require(decode(wrong, packet)
            && assembler.accept(packet, receiver, start).status == Reassembler::Status::Invalid,
        "nonconsecutive offset rejected");
    wrong = second.bytes;
    field(wrong, 0, 8);
    require(decode(wrong, packet)
            && assembler.accept(packet, receiver, start).status == Reassembler::Status::Invalid,
        "interleaved serial rejected");
    require(decode(second.bytes, packet)
            && assembler.accept(packet, receiver, start + std::chrono::seconds(29)).status
                == Reassembler::Status::Pending,
        "continuation before deadline");
    require(!assembler.expired(start + std::chrono::seconds(29))
            && assembler.expired(start + std::chrono::seconds(30)),
        "fixed first-fragment deadline");
    auto third = encode_next(route, 7, bytes, second.next_offset);
    require(decode(third.bytes, packet)
            && assembler.accept(packet, receiver, start + std::chrono::seconds(30)).status
                == Reassembler::Status::Invalid,
        "continuation cannot renew deadline");
    assembler.close(route);
    receiver.close(route);
    require(!assembler.active(route) && !assembler.expired(start + std::chrono::seconds(31)),
        "close drops partial storage and serial ownership");
    require(assembler.accept(packet, receiver).status == Reassembler::Status::Invalid,
        "retired native capability rejected");
    Ledger quota;
    auto full = admit(quota);
    for (size_t index = 0; index < 16; ++index)
        require(quota.received(full, 1), "logical quota setup");
    auto denied = encode_next(full, 1, bytes, 0);
    require(decode(denied.bytes, packet)
            && assembler.accept(packet, quota).status == Reassembler::Status::Invalid
            && !assembler.active(full),
        "receive quota checked before reassembly allocation");
    assembler.clear();
}

int main()
{
    fragments();
    Ledger sender, receiver;
    std::vector<Identity> ids;
    require(sender.prepare_transfer(2, ids), "prepare native identities");
    auto frame = encode(Kind::Transfer, {}, ids, std::span<char const>("graph", 5));
    Packet parsed {};
    require(decode(frame, parsed) && parsed.ports.size() == 2 && parsed.payload.size() == 5,
        "transfer roundtrip");
    require(
        sender.admit_transfer(ids) && receiver.admit_transfer(parsed.ports), "symmetric admission");
    require(!receiver.admit_transfer(parsed.ports), "replay cannot introduce route");
    auto malformed = frame;
    malformed[6] = 1;
    require(!decode(malformed, parsed), "reserved flags rejected");
    malformed = frame;
    malformed[5] = 17;
    require(!decode(malformed, parsed), "attachment quota rejected");
    malformed = frame;
    for (size_t i = 0; i < 8; ++i)
        malformed[24 + i] = malformed[16 + i];
    require(!decode(malformed, parsed), "duplicate native ID rejected");
    require(!decode(encode(Kind::Data, ids[0], {}, {}), parsed), "empty data rejected");
    require(!decode(encode(Kind::Close, ids[0], {}, std::span<char const>("x", 1)), parsed),
        "close payload rejected");
    require(sender.state({ ids[0].id, 2 }) == Ledger::State::Invalid, "forged generation rejected");
    require(sender.state({ 99, 1 }) == Ledger::State::Invalid, "unissued identity rejected");
    for (size_t i = 0; i < 16; ++i) {
        require(sender.can_send(ids[0], 65536) && receiver.received(ids[0], 65536),
            "genuine directional window");
        sender.sent(ids[0], 65536);
    }

    require(!sender.can_send(ids[0], 1) && !receiver.received(ids[0], 1), "16-message/1MiB limit");
    require(!sender.acknowledge(ids[0], 1) && !receiver.can_consume(ids[0], 1),
        "wrong ACK does not refund");
    require(receiver.can_consume(ids[0], 65536), "FIFO receive debit");
    receiver.consumed(ids[0], 65536);
    require(sender.acknowledge(ids[0], 65536) && sender.can_send(ids[0], 1),
        "exact ACK refunds credit");
    sender.close(ids[0]);
    receiver.close(ids[0]);
    require(sender.state(ids[0]) == Ledger::State::Retired && !sender.can_send(ids[0], 1),
        "closed route cannot resurrect");
    require(sender.prepare_transfer(1, ids) && ids[0].id == 3 && sender.admit_transfer(ids),
        "monotonic identity after close");
    Ledger quota;
    for (size_t i = 0; i < 16; ++i) {
        require(
            quota.prepare_transfer(16, ids) && quota.admit_transfer(ids), "route quota filling");
    }

    require(!quota.prepare_transfer(1, ids), "256 active routes bounded");
    for (uint32_t id = 1; id <= 4; ++id)
        for (size_t i = 0; i < 16; ++i) {
            require(quota.can_send({ id, 1 }, 65536), "endpoint quota filling");
            quota.sent({ id, 1 }, 65536);
        }

    require(!quota.can_send({ 5, 1 }, 1), "endpoint TX4MiB bound");
    // A peer can send its full window while ours remains unacknowledged.
    for (uint32_t id = 1; id <= 4; ++id)
        for (size_t i = 0; i < 16; ++i)
            require(quota.received({ id, 1 }, 65536), "simultaneous duplex RX4MiB admitted");
    require(!quota.received({ 5, 1 }, 1), "combined endpoint8MiB bound");
    quota.close({ 1, 1 });
    require(quota.can_send({ 9, 1 }, 65536), "close releases outstanding bytes");
    quota.retire_all();
    require(quota.state({ 256, 1 }) == Ledger::State::Retired && quota.prepare_transfer(1, ids)
            && ids[0].id == 257,
        "channel cleanup preserves identity tombstone watermark");
    quota.clear();
    require(quota.state({ 1, 1 }) == Ledger::State::Invalid && quota.prepare_transfer(1, ids)
            && ids[0].id == 1,
        "closed channel discards ledger");
    std::puts("PASS native utility port codec and exact directional/endpoint credit ownership");
}
