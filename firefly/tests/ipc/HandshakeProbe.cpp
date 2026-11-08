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
    for (size_t offset : { 0u, 4u, 5u, 6u, 7u }) {
        auto invalid = TRY(ByteBuffer::copy(frame.bytes()));
        invalid[offset] = offset == 4 ? 6 : offset == 5 ? 2 : invalid[offset] ^ 1;
        if (!decode_message(invalid.bytes()).is_error())
            return Error::from_string_literal("Invalid application frame tag/flags accepted");
    }

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
