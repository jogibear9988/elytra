/** Bounded private byte-channel framing. No JavaScript, JSON or implicit text conversion. */
#pragma once
#include <AK/Error.h>
#include <AK/Span.h>
#include <LibIPC/Message.h>

namespace Firefly::Bytes {
inline constexpr size_t header_size = 20;
inline constexpr size_t max_payload = 128 * 1024;
inline constexpr size_t max_pending = 16;
inline constexpr size_t max_queued_bytes = max_payload * max_pending;

enum class PacketKind : u8 { Data = 0, Ack = 1 };

struct Packet {
    PacketKind kind;
    u64 sequence;
    ReadonlyBytes payload;
};

ErrorOr<IPC::MessageBuffer> encode_packet(PacketKind, u64 sequence, ReadonlyBytes);

ErrorOr<Packet> decode_packet(ReadonlyBytes);
}
