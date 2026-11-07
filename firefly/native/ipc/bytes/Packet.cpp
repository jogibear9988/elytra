/** Validate lengths and fixed-width fields before allocating or interpreting a payload. */
#include "Packet.h"
#include <AK/Array.h>

namespace Firefly::Bytes {
ErrorOr<IPC::MessageBuffer> encode_packet(PacketKind kind, u64 sequence, ReadonlyBytes payload)
{
    if (!sequence || payload.size() > max_payload
        || (kind != PacketKind::Data && kind != PacketKind::Ack)
        || (kind == PacketKind::Ack && !payload.is_empty()))
        return Error::from_string_literal("Invalid byte-channel packet");
    Array<u8, header_size> header {};
    header[0] = 'F';
    header[1] = 'F';
    header[2] = 'B';
    header[3] = 'C';
    header[4] = 1;
    header[5] = static_cast<u8>(kind);
    for (size_t i = 0; i < 8; ++i)
        header[8 + i] = static_cast<u8>(sequence >> (i * 8));
    for (size_t i = 0; i < 4; ++i)
        header[16 + i] = static_cast<u8>(payload.size() >> (i * 8));
    IPC::MessageBuffer output;
    TRY(output.extend_data_capacity(header_size + payload.size()));
    TRY(output.append_data(header.data(), header.size()));
    if (!payload.is_empty())
        TRY(output.append_data(payload.data(), payload.size()));
    return output;
}

ErrorOr<Packet> decode_packet(ReadonlyBytes bytes)
{
    if (bytes.size() < header_size || bytes.size() > header_size + max_payload || bytes[0] != 'F'
        || bytes[1] != 'F' || bytes[2] != 'B' || bytes[3] != 'C' || bytes[4] != 1 || bytes[5] > 1
        || bytes[6] || bytes[7])
        return Error::from_string_literal("Invalid byte-channel header");
    u64 sequence = 0;
    u32 size = 0;
    for (size_t i = 0; i < 8; ++i)
        sequence |= static_cast<u64>(bytes[8 + i]) << (i * 8);
    for (size_t i = 0; i < 4; ++i)
        size |= static_cast<u32>(bytes[16 + i]) << (i * 8);
    auto kind = static_cast<PacketKind>(bytes[5]);
    if (!sequence || size != bytes.size() - header_size || (kind == PacketKind::Ack && size))
        return Error::from_string_literal("Invalid byte-channel sequence/length");
    return Packet { kind, sequence, bytes.slice(header_size) };
}
}
