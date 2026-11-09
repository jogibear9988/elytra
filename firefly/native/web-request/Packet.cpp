/** Explicit little-endian framing; transport messages supply the bounded packet length. */
#include "Packet.h"
#include <AK/JsonValue.h>
#include <AK/String.h>

namespace Firefly::WebRequest {
namespace {
    u64 read_word(ReadonlyBytes bytes, size_t offset, size_t length)
    {
        u64 result = 0;
        for (size_t i = 0; i < length; ++i)
            result |= static_cast<u64>(bytes[offset + i]) << (8 * i);
        return result;
    }

    void write_word(Bytes bytes, size_t offset, size_t length, u64 value)
    {
        for (size_t i = 0; i < length; ++i)
            bytes[offset + i] = static_cast<u8>(value >> (8 * i));
    }

    ErrorOr<void> validate(Packet const& packet)
    {
        if (static_cast<u8>(packet.kind) > static_cast<u8>(Kind::Finish)
            || (packet.kind == Kind::Headers && packet.flags != 1) || packet.flags > 1
            || (packet.flags
                && (packet.kind == Kind::Hello || packet.kind == Kind::Policy
                    || packet.kind == Kind::Before))
            || packet.epoch > 2147483647 || packet.request > 2147483647)
            return Error::from_string_literal("Invalid web request packet fields");
        bool identity = packet.page || packet.navigable_namespace || packet.navigable_local
            || packet.document_namespace || packet.document_local;
        if (packet.kind == Kind::Hello) {
            if (packet.epoch || packet.request || identity)
                return Error::from_string_literal("Invalid web request hello owner");
        } else if (packet.kind == Kind::Policy) {
            if (!packet.epoch || packet.request || identity)
                return Error::from_string_literal("Invalid web request policy owner");
        } else if (!packet.epoch || !packet.request || !packet.navigable_namespace
            || !packet.navigable_local || !packet.document_namespace || !packet.document_local) {
            return Error::from_string_literal("Invalid web request document owner");
        }

        return {};
    }
}

ErrorOr<ByteBuffer> encode_packet(Packet const& packet)
{
    TRY(validate(packet));
    auto payload = packet.payload.serialized();
    if (payload.byte_count() > packet_payload_limit)
        return Error::from_string_literal("Web request metadata limit exceeded");
    auto output = TRY(ByteBuffer::create_zeroed(packet_header_size + payload.byte_count()));
    output[0] = 'F';
    output[1] = 'W';
    output[2] = 'R';
    output[3] = '1';
    output[4] = static_cast<u8>(packet.kind);
    output[5] = packet.flags;
    write_word(output, 8, 4, packet.epoch);
    write_word(output, 12, 4, packet.request);
    write_word(output, 16, 8, packet.page);
    write_word(output, 24, 8, packet.navigable_namespace);
    write_word(output, 32, 8, packet.navigable_local);
    write_word(output, 40, 8, packet.document_namespace);
    write_word(output, 48, 8, packet.document_local);
    __builtin_memcpy(
        output.data() + packet_header_size, payload.bytes().data(), payload.byte_count());
    return output;
}

ErrorOr<Packet> decode_packet(ReadonlyBytes bytes)
{
    if (bytes.size() < packet_header_size
        || bytes.size() > packet_header_size + packet_payload_limit || bytes[0] != 'F'
        || bytes[1] != 'W' || bytes[2] != 'R' || bytes[3] != '1' || bytes[6] || bytes[7])
        return Error::from_string_literal("Invalid web request packet framing");
    auto payload = TRY(String::from_utf8(bytes.slice(packet_header_size)));
    auto value = TRY(JsonValue::from_string(payload));
    if (!value.is_object())
        return Error::from_string_literal("Web request metadata must be an object");
    Packet packet { static_cast<Kind>(bytes[4]), bytes[5], static_cast<u32>(read_word(bytes, 8, 4)),
        static_cast<u32>(read_word(bytes, 12, 4)), read_word(bytes, 16, 8), read_word(bytes, 24, 8),
        read_word(bytes, 32, 8), read_word(bytes, 40, 8), read_word(bytes, 48, 8),
        value.as_object() };
    TRY(validate(packet));
    return packet;
}
}
