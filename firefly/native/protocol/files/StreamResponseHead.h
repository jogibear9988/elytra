/** Separate streamed-response schema: no file path, descriptor or native capability. */
#pragma once
#include "Limits.h"
#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/Optional.h>
#include <AK/Vector.h>
#include <LibHTTP/Header.h>

namespace Firefly::Protocols {
struct StreamResponseHead {
    u32 request;
    u16 status;
    Optional<u64> content_length;
    bool head;
    bool body_null;
    Vector<HTTP::Header> headers;
};

// HEAD and null-body statuses carry no stream. An advertised representation length
// is permitted for HEAD/304; it never authorizes body bytes on those responses.
ErrorOr<ByteString> encode_stream_response_head(StreamResponseHead const&);

ErrorOr<StreamResponseHead> decode_stream_response_head(StringView);
}
