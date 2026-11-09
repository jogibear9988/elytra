/** Private packet schema shared by the authenticated UI/helper control channel. */
#pragma once
#include <AK/ByteBuffer.h>
#include <AK/JsonObject.h>

namespace Firefly::WebRequest {
enum class Kind : u8 { Hello, Policy, Before, Headers, Decision, Cancel, Finish };
constexpr size_t packet_header_size = 56;
constexpr size_t packet_payload_limit = 96 * 1024;

struct Packet {
    Kind kind;
    u8 flags { 0 };
    u32 epoch { 0 };
    u32 request { 0 };
    u64 page { 0 };
    u64 navigable_namespace { 0 };
    u64 navigable_local { 0 };
    u64 document_namespace { 0 };
    u64 document_local { 0 };
    JsonObject payload;
};

ErrorOr<ByteBuffer> encode_packet(Packet const&);

ErrorOr<Packet> decode_packet(ReadonlyBytes);
}
