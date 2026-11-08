/** Independent native readiness-frame geometry, arbitrary byte fields and malformed input checks.
 */
#include "native/ipc/application/Handshake.h"
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
    outln("PASS native document IPC hello: independent bytes, exact identities, "
          "truncation/zero/version/size refusal");
    return 0;
}
