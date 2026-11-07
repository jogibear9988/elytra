/** Versioned response frames correlate bounded metadata/body/control with one native owner. */
#pragma once
#include "Limits.h"
#include <AK/ByteBuffer.h>

namespace Firefly::Protocols {
inline constexpr size_t file_packet_header_size = 32;
enum class FilePacketKind : u8 { Metadata = 1, Chunk = 2, Complete = 3, Cancel = 4 };

struct FilePacket {
    FilePacketKind kind;
    u64 epoch;
    u32 request;
    u64 offset;
    ReadonlyBytes payload;
};

// The decoded payload borrows the input only. Epoch/ID identify, not authorize.
ErrorOr<FilePacket> decode_file_packet(ReadonlyBytes);

ErrorOr<ByteBuffer> encode_file_packet(FilePacket const&);
}
