/** Fixed little-endian framing. No native structs, paths or implicit platform ABI on the wire. */
#include "FilePacket.h"
#include "ResponseHead.h"

namespace Firefly::Protocols {
namespace {
    u64 word(ReadonlyBytes input, size_t offset, size_t width)
    {
        u64 value = 0;
        for (size_t i = 0; i < width; ++i)
            value |= static_cast<u64>(input[offset + i]) << (i * 8);
        return value;
    }

    void write_word(Bytes output, size_t offset, size_t width, u64 value)
    {
        for (size_t i = 0; i < width; ++i)
            output[offset + i] = static_cast<u8>(value >> (i * 8));
    }

    ErrorOr<void> validate(FilePacket const& packet)
    {
        if (!packet.epoch || !packet.request || packet.offset > file_size_limit)
            return Error::from_string_literal("Invalid file packet owner/request/offset");
        switch (packet.kind) {
        case FilePacketKind::Metadata:
            if (!packet.offset && !packet.payload.is_empty()
                && packet.payload.size() <= response_head_limit)
                return {};
            break;
        case FilePacketKind::Chunk:
            if (!packet.payload.is_empty() && packet.payload.size() <= file_chunk_limit
                && packet.payload.size() <= file_size_limit - packet.offset)
                return {};
            break;
        case FilePacketKind::Complete:
            if (packet.payload.is_empty())
                return {};
            break;
        case FilePacketKind::Cancel:
            if (packet.payload.is_empty() && !packet.offset)
                return {};
            break;
        }

        return Error::from_string_literal("Invalid file packet kind/payload/offset");
    }
}

ErrorOr<FilePacket> decode_file_packet(ReadonlyBytes bytes)
{
    if (bytes.size() < file_packet_header_size || bytes[0] != 'F' || bytes[1] != 'F'
        || bytes[2] != 'R' || bytes[3] != 'P' || bytes[4] != 1 || bytes[6] || bytes[7])
        return Error::from_string_literal("Unknown/truncated file packet header");
    auto length = word(bytes, 20, 4);
    if (length != bytes.size() - file_packet_header_size)
        return Error::from_string_literal("File packet length mismatch");
    FilePacket packet { static_cast<FilePacketKind>(bytes[5]), word(bytes, 8, 8),
        static_cast<u32>(word(bytes, 16, 4)), word(bytes, 24, 8),
        bytes.slice(file_packet_header_size) };
    TRY(validate(packet));
    return packet;
}

ErrorOr<ByteBuffer> encode_file_packet(FilePacket const& packet)
{
    TRY(validate(packet));
    auto output = TRY(ByteBuffer::create_zeroed(file_packet_header_size + packet.payload.size()));
    output[0] = 'F';
    output[1] = 'F';
    output[2] = 'R';
    output[3] = 'P';
    output[4] = 1;
    output[5] = static_cast<u8>(packet.kind);
    write_word(output, 8, 8, packet.epoch);
    write_word(output, 16, 4, packet.request);
    write_word(output, 20, 4, packet.payload.size());
    write_word(output, 24, 8, packet.offset);
    if (!packet.payload.is_empty())
        __builtin_memcpy(
            output.data() + file_packet_header_size, packet.payload.data(), packet.payload.size());
    return output;
}
}
