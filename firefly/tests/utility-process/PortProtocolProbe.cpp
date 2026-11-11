/** Pure native protocol/accounting controls; no VM, child, network, or OS handle is fabricated. */
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

int main()
{
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
