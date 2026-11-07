/** Bounded renderer-to-main requests. Epoch/ID identify an admitted channel, never authorize it. */
#pragma once
#include "Limits.h"
#include <AK/ByteBuffer.h>
#include <AK/Error.h>

namespace Firefly::Protocols {
inline constexpr size_t file_request_header_size = 32;
inline constexpr size_t file_request_url_limit = 8192;

enum class FileRequestKind : u8 { Get = 1, Head = 2, Credit = 3, Cancel = 4 };

struct FileRequestPacket {
    FileRequestKind kind;
    u64 epoch;
    u32 request;
    u64 credit;
    // Borrowed UTF-8 URL bytes, only for Get/Head. No path or arbitrary headers.
    ReadonlyBytes url;
};

ErrorOr<FileRequestPacket> decode_file_request(ReadonlyBytes);

ErrorOr<ByteBuffer> encode_file_request(FileRequestPacket const&);
}
