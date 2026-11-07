/** Fixed little-endian request framing, distinct from response frames and native structs. */
#include "RequestPacket.h"
#include <AK/Utf8View.h>

namespace Firefly::Protocols {
namespace {
    u64 word(ReadonlyBytes bytes, size_t offset, size_t width)
    {
        u64 value = 0;
        for (size_t i = 0; i < width; ++i)
            value |= static_cast<u64>(bytes[offset + i]) << (8 * i);
        return value;
    }

    void write(Bytes bytes, size_t offset, size_t width, u64 value)
    {
        for (size_t i = 0; i < width; ++i)
            bytes[offset + i] = static_cast<u8>(value >> (8 * i));
    }

    ErrorOr<void> validate(FileRequestPacket const& packet)
    {
        if (!packet.epoch || !packet.request)
            return Error::from_string_literal("Invalid file request identity");
        switch (packet.kind) {
        case FileRequestKind::Get:
        case FileRequestKind::Head: {
            auto url = StringView { reinterpret_cast<char const*>(packet.url.data()),
                packet.url.size() };
            if (!packet.credit && !url.is_empty() && url.length() <= file_request_url_limit
                && !url.contains('\0') && Utf8View(url).validate())
                return {};
            break;
        }

        case FileRequestKind::Credit:
            if (packet.url.is_empty() && packet.credit && packet.credit <= file_chunk_limit)
                return {};
            break;
        case FileRequestKind::Cancel:
            if (packet.url.is_empty() && !packet.credit)
                return {};
            break;
        }

        return Error::from_string_literal("Invalid file request kind/URL/credit");
    }
}

ErrorOr<FileRequestPacket> decode_file_request(ReadonlyBytes bytes)
{
    if (bytes.size() < file_request_header_size || bytes[0] != 'F' || bytes[1] != 'F'
        || bytes[2] != 'Q' || bytes[3] != 'P' || bytes[4] != 1 || bytes[6] || bytes[7]
        || word(bytes, 20, 4) != bytes.size() - file_request_header_size)
        return Error::from_string_literal("Invalid file request header");
    FileRequestPacket packet { static_cast<FileRequestKind>(bytes[5]), word(bytes, 8, 8),
        static_cast<u32>(word(bytes, 16, 4)), word(bytes, 24, 8),
        bytes.slice(file_request_header_size) };
    TRY(validate(packet));
    return packet;
}

ErrorOr<ByteBuffer> encode_file_request(FileRequestPacket const& packet)
{
    TRY(validate(packet));
    auto bytes = TRY(ByteBuffer::create_zeroed(file_request_header_size + packet.url.size()));
    bytes[0] = 'F';
    bytes[1] = 'F';
    bytes[2] = 'Q';
    bytes[3] = 'P';
    bytes[4] = 1;
    bytes[5] = static_cast<u8>(packet.kind);
    write(bytes, 8, 8, packet.epoch);
    write(bytes, 16, 4, packet.request);
    write(bytes, 20, 4, packet.url.size());
    write(bytes, 24, 8, packet.credit);
    if (!packet.url.is_empty())
        __builtin_memcpy(
            bytes.data() + file_request_header_size, packet.url.data(), packet.url.size());
    return bytes;
}
}
