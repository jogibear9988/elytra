/** Raw application frames travel only on an already native-bound document channel. */
#pragma once
#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/Span.h>

namespace Firefly::ApplicationIpc {
constexpr size_t message_header_size = 20;
constexpr size_t wire_limit = 128 * 1024 - message_header_size;
constexpr size_t queue_limit = 48;
enum class Kind : u8 { Invoke = 1, Send, Reply, Event, Ack };

struct Message {
    Kind kind;
    bool success;
    u64 epoch;
    u32 request;
    ReadonlyBytes payload;
};

ErrorOr<ByteString> encode_message(Kind, bool success, u64 epoch, u32 request, ReadonlyBytes);

ErrorOr<Message> decode_message(ReadonlyBytes);
}
