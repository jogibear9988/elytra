/** Independent native readiness-frame geometry, arbitrary byte fields and malformed input checks.
 */
#include "native/ipc/application/Handshake.h"
#include "native/ipc/application/Message.h"
#include <AK/ByteBuffer.h>
#include <AK/Format.h>
#include <LibMain/Main.h>

ErrorOr<int> ladybird_main(Main::Arguments)
{
    using namespace Firefly::ApplicationIpc;
    Hello value { 0xff00800102030405ULL, 0x1122334455667788ULL, 1, 2, 3, 4 };
    auto encoded = encode_hello(value);
    // Fixed little-endian vector independent from decode_hello's implementation.
    Array<u8, 20> expected { 'F', 'I', 'H', '1', 5, 4, 3, 2, 1, 128, 0, 255, 136, 119, 102, 85, 68,
        51, 34, 17 };
    for (size_t i = 0; i < expected.size(); ++i)
        if (encoded[i] != expected[i])
            return Error::from_string_literal("Native hello byte layout changed");
    auto decoded = TRY(decode_hello(encoded.span()));
    if (decoded.epoch != value.epoch || decoded.pid != value.pid || decoded.document_namespace != 1
        || decoded.document_local != 2 || decoded.navigable_namespace != 3
        || decoded.navigable_local != 4)
        return Error::from_string_literal("Native hello identity changed");
    for (size_t length = 0; length < encoded.size(); ++length)
        if (!decode_hello(encoded.span().slice(0, length)).is_error())
            return Error::from_string_literal("Truncated hello accepted");
    for (size_t field = 0; field < 6; ++field) {
        auto invalid = encoded;
        for (size_t i = 0; i < 8; ++i)
            invalid[4 + field * 8 + i] = 0;
        if (!decode_hello(invalid.span()).is_error())
            return Error::from_string_literal("Zero identity hello accepted");
    }

    encoded[0] ^= 1;
    if (!decode_hello(encoded.span()).is_error())
        return Error::from_string_literal("Wrong hello version accepted");
    Array<u8, hello_size + 1> oversized {};
    if (!decode_hello(oversized.span()).is_error())
        return Error::from_string_literal("Oversized hello accepted");
    Array<u8, 3> payload { 0, 128, 255 };
    auto frame = TRY(
        encode_message(Kind::Invoke, false, 0xff00800102030405ULL, 0xaabbccdd, payload.span()));
    Array<u8, 20> fixed { 'F', 'I', 'M', '1', 1, 0, 0, 0, 5, 4, 3, 2, 1, 128, 0, 255, 221, 204, 187,
        170 };
    for (size_t i = 0; i < fixed.size(); ++i)
        if (frame.bytes()[i] != fixed[i])
            return Error::from_string_literal("Application frame layout changed");
    auto message = TRY(decode_message(frame.bytes()));
    if (message.kind != Kind::Invoke || message.request != 0xaabbccdd
        || message.epoch != value.epoch || message.payload.size() != 3 || message.payload[2] != 255)
        return Error::from_string_literal("Application frame round trip failed");
    for (size_t length = 0; length <= message_header_size; ++length)
        if (!decode_message(frame.bytes().slice(0, length)).is_error())
            return Error::from_string_literal("Truncated application frame accepted");
    auto ack = TRY(encode_message(Kind::Ack, false, 1, 1, {}));
    TRY(decode_message(ack.bytes()));
    if (!encode_message(Kind::Ack, false, 1, 1, payload.span()).is_error()
        || !encode_message(Kind::Invoke, true, 1, 1, payload.span()).is_error()
        || !encode_message(Kind::Reply, false, 0, 1, payload.span()).is_error()
        || !encode_message(Kind::Event, false, 1, 0, payload.span()).is_error())
        return Error::from_string_literal("Invalid application frame fields accepted");
    // Tags 6/7 are now the authenticated port envelope/transfer ACK, not
    // malformed tags. Exercise the entire byte domain so another legitimate
    // schema extension cannot silently leave a stale single-tag negative test.
    Array<Kind, 7> kinds { Kind::Invoke, Kind::Send, Kind::Reply, Kind::Event, Kind::Ack,
        Kind::PortControl, Kind::PortAck };
    for (size_t i = 0; i < kinds.size(); ++i)
        if (static_cast<size_t>(to_underlying(kinds[i])) != i + 1)
            return Error::from_string_literal("Application frame kind layout changed");
    auto nonempty = TRY(ByteBuffer::copy(frame.bytes()));
    auto empty = TRY(ByteBuffer::copy(frame.bytes().slice(0, message_header_size)));
    for (unsigned tag = 0; tag < 256; ++tag) {
        for (unsigned flags = 0; flags < 256; ++flags) {
            for (bool has_payload : { false, true }) {
                auto& candidate = has_payload ? nonempty : empty;
                candidate[4] = static_cast<u8>(tag);
                candidate[5] = static_cast<u8>(flags);
                bool valid = tag >= 1 && tag <= 7
                    && (flags == 0 || (tag == to_underlying(Kind::Reply) && flags == 1))
                    && ((tag == to_underlying(Kind::Ack) || tag == to_underlying(Kind::PortAck))
                            ? !has_payload
                            : has_payload);
                if (decode_message(candidate.bytes()).is_error() == valid)
                    return Error::from_string_literal(
                        "Application frame tag/flag/payload matrix mismatch");
                if (flags < 2) {
                    auto generated = encode_message(static_cast<Kind>(tag), flags != 0, value.epoch,
                        0xaabbccdd, has_payload ? payload.span() : ReadonlyBytes {});
                    if (generated.is_error() == valid)
                        return Error::from_string_literal(
                            "Application encoder tag/flag/payload matrix mismatch");
                }
            }
        }
    }

    for (size_t offset : { 6u, 7u }) {
        auto invalid = TRY(ByteBuffer::copy(frame.bytes()));
        for (unsigned reserved = 1; reserved < 256; ++reserved) {
            invalid[offset] = static_cast<u8>(reserved);
            if (!decode_message(invalid.bytes()).is_error())
                return Error::from_string_literal("Reserved application frame flags accepted");
        }
    }

    auto bad_magic = TRY(ByteBuffer::copy(frame.bytes()));
    bad_magic[0] ^= 1;
    if (!decode_message(bad_magic.bytes()).is_error())
        return Error::from_string_literal("Invalid application frame magic accepted");

    auto maximum = TRY(ByteBuffer::create_zeroed(wire_limit));
    auto max_frame = TRY(encode_message(Kind::Event, false, 1, 1, maximum.bytes()));
    TRY(decode_message(max_frame.bytes()));
    auto too_large = TRY(ByteBuffer::create_zeroed(wire_limit + 1));
    if (!encode_message(Kind::Event, false, 1, 1, too_large.bytes()).is_error())
        return Error::from_string_literal("Oversized application payload accepted");
    outln("PASS native document IPC hello: independent bytes, exact identities, "
          "truncation/zero/version/size refusal");
    return 0;
}
