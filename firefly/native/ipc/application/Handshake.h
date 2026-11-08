/** Fixed raw readiness proof for one native document channel; no JSON or application metadata. */
#pragma once
#include <AK/Array.h>
#include <AK/Error.h>
#include <AK/Span.h>

namespace Firefly::ApplicationIpc {
constexpr size_t hello_size = 52;

struct Hello {
    u64 epoch;
    u64 pid;
    u64 document_namespace;
    u64 document_local;
    u64 navigable_namespace;
    u64 navigable_local;
};

Array<u8, hello_size> encode_hello(Hello const&);

ErrorOr<Hello> decode_hello(ReadonlyBytes);
}
