/** Native little-endian fields are read with bounds checks; an endpoint must check their identity.
 */
#include "Handshake.h"

namespace Firefly::ApplicationIpc {
Array<u8, hello_size> encode_hello(Hello const& hello)
{
    Array<u8, hello_size> bytes {};
    __builtin_memcpy(bytes.data(), "FIH1", 4);
    Array<u64, 6> words { hello.epoch, hello.pid, hello.document_namespace, hello.document_local,
        hello.navigable_namespace, hello.navigable_local };
    for (size_t i = 0; i < words.size(); ++i)
        for (size_t j = 0; j < 8; ++j)
            bytes[4 + i * 8 + j] = static_cast<u8>(words[i] >> (j * 8));
    return bytes;
}

ErrorOr<Hello> decode_hello(ReadonlyBytes bytes)
{
    if (bytes.size() != hello_size || __builtin_memcmp(bytes.data(), "FIH1", 4))
        return Error::from_string_literal("Invalid application IPC hello");
    Array<u64, 6> words {};
    for (size_t i = 0; i < words.size(); ++i) {
        for (size_t j = 0; j < 8; ++j)
            words[i] |= static_cast<u64>(bytes[4 + i * 8 + j]) << (j * 8);
        if (!words[i])
            return Error::from_string_literal("Zero application IPC hello identity");
    }

    return Hello { words[0], words[1], words[2], words[3], words[4], words[5] };
}
}
