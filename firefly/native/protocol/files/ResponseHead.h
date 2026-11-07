/** Internal file-response metadata. No path or native file capability crosses this boundary. */
#pragma once
#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/Span.h>
#include <AK/Vector.h>
#include <LibHTTP/Header.h>

namespace Firefly::Protocols {
inline constexpr size_t response_head_limit = 32 * 1024;
inline constexpr size_t response_header_limit = 64;

struct ResponseHead {
    u32 request;
    u64 content_length;
    bool head;
    Vector<HTTP::Header> headers;
};

// Derive MIME from the already authorized filename, unless explicitly supplied.
// Generated Content-Length always agrees with the opened descriptor's length.
ErrorOr<ResponseHead> make_file_response_head(u32 request, u64 length, bool head,
    StringView authorized_path, ReadonlySpan<HTTP::Header> supplied = {});

ErrorOr<ByteString> encode_file_response_head(ResponseHead const&);

ErrorOr<ResponseHead> decode_file_response_head(StringView);
}
