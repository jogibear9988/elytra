/** Validate fixed framing before looking at the bounded raw graph; no UTF-8 byte conversion. */
#include "Message.h"
#include <AK/ByteBuffer.h>

namespace Firefly::ApplicationIpc {
ErrorOr<ByteString> encode_message(
    Kind kind, bool success, u64 epoch, u32 request, ReadonlyBytes payload)
{
    if (!epoch || !request || payload.size() > wire_limit || (kind != Kind::Reply && success)
        || (kind == Kind::Ack ? !payload.is_empty() : payload.is_empty()) || to_underlying(kind) < 1
        || to_underlying(kind) > 5)
        return Error::from_string_literal("Invalid application frame fields");
    auto bytes = TRY(ByteBuffer::create_zeroed(message_header_size + payload.size()));
    __builtin_memcpy(bytes.data(), "FIM1", 4);
    bytes[4] = to_underlying(kind);
    bytes[5] = success;
    for (size_t i = 0; i < 8; ++i)
        bytes[8 + i] = static_cast<u8>(epoch >> (i * 8));
    for (size_t i = 0; i < 4; ++i)
        bytes[16 + i] = static_cast<u8>(request >> (i * 8));
    if (!payload.is_empty())
        __builtin_memcpy(bytes.data() + message_header_size, payload.data(), payload.size());
    return ByteString(StringView(reinterpret_cast<char const*>(bytes.data()), bytes.size()));
}

ErrorOr<Message> decode_message(ReadonlyBytes bytes)
{
    if (bytes.size() < message_header_size || bytes.size() > message_header_size + wire_limit
        || __builtin_memcmp(bytes.data(), "FIM1", 4) || bytes[4] < 1 || bytes[4] > 5 || bytes[5] > 1
        || bytes[6] || bytes[7])
        return Error::from_string_literal("Invalid application frame header");
    u64 epoch = 0;
    u32 request = 0;
    for (size_t i = 0; i < 8; ++i)
        epoch |= static_cast<u64>(bytes[8 + i]) << (i * 8);
    for (size_t i = 0; i < 4; ++i)
        request |= static_cast<u32>(bytes[16 + i]) << (i * 8);
    auto kind = static_cast<Kind>(bytes[4]);
    auto payload = bytes.slice(message_header_size);
    if (!epoch || !request || (kind != Kind::Reply && bytes[5])
        || (kind == Kind::Ack ? !payload.is_empty() : payload.is_empty()))
        return Error::from_string_literal("Invalid application frame identity/payload");
    return Message { kind, bytes[5] != 0, epoch, request, payload };
}
}
