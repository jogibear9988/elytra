/** Bounded logical application messages over unchanged 128 KiB byte-channel frames. */
#pragma once
#include "Message.h"
#include <AK/ByteStringImpl.h>
#include <AK/Optional.h>
#include <AK/RefPtr.h>

namespace Firefly::ApplicationIpc {
constexpr size_t fragment_header_size = 32;
constexpr size_t fragment_payload_limit = 128 * 1024 - fragment_header_size;

struct Fragment {
    ByteString bytes;
    size_t next_offset;
    bool complete;
};

struct OwnedMessage {
    Kind kind;
    bool success;
    u64 epoch;
    u32 request;
    ByteString payload;
};

class FragmentEncoder {
public:
    static ErrorOr<Fragment> encode_next(
        Kind, bool success, u64 epoch, u32 request, ReadonlyBytes wire, size_t offset);
};

class Reassembler {
public:
    Reassembler() = default;
    Reassembler(Reassembler const&) = delete;
    Reassembler& operator=(Reassembler const&) = delete;
    Reassembler(Reassembler&&);

    Reassembler& operator=(Reassembler&&);
    // Pure header preflight. The caller reserves this entire logical size against
    // document/process quotas BEFORE accept allocates; its lease must follow a
    // completed OwnedMessage until dispatch/decoding has released the bytes.
    static ErrorOr<size_t> required_bytes(ReadonlyBytes, u64 expected_epoch);

    ErrorOr<Optional<OwnedMessage>> accept(ReadonlyBytes, u64 expected_epoch);

    void reset();

    bool active() const
    {
        return m_total != 0;
    }

    size_t byte_count() const
    {
        return m_total;
    }

    size_t retained_bytes() const
    {
        return byte_count();
    }

private:
    Kind m_kind { Kind::Ack };
    bool m_success {};
    u64 m_epoch {};
    u32 m_request {};
    size_t m_total {};
    size_t m_offset {};
    // Unpublished storage: only contiguous received bytes are written; no
    // ByteString/value is exposed until every payload byte has been initialized.
    RefPtr<AK::ByteStringImpl const> m_storage;
    char* m_write {};
};

using FragmentReassembler = Reassembler;
}
