/** Strict single-message reassembly; document authentication and deadlines stay with its caller. */
#include "Fragment.h"
#include <algorithm>

namespace Firefly::ApplicationIpc {
namespace {
    constexpr size_t frame_limit = 128 * 1024;

    u32 read32(ReadonlyBytes bytes, size_t offset)
    {
        u32 result = 0;
        for (size_t i = 0; i < 4; ++i)
            result |= static_cast<u32>(bytes[offset + i]) << (8 * i);
        return result;
    }

    u64 read64(ReadonlyBytes bytes, size_t offset)
    {
        u64 result = 0;
        for (size_t i = 0; i < 8; ++i)
            result |= static_cast<u64>(bytes[offset + i]) << (8 * i);
        return result;
    }

    void write32(Bytes bytes, size_t offset, u32 value)
    {
        for (size_t i = 0; i < 4; ++i)
            bytes[offset + i] = static_cast<u8>(value >> (8 * i));
    }

    struct Frame {
        Kind kind;
        bool success;
        u64 epoch;
        u32 request;
        ReadonlyBytes payload;
        bool fragmented;
        size_t total;
        size_t offset;
    };

    ErrorOr<Frame> parse(ReadonlyBytes bytes, u64 expected_epoch)
    {
        if (!expected_epoch || bytes.size() < 4)
            return Error::from_string_literal("Invalid application fragment epoch/header");
        if (__builtin_memcmp(bytes.data(), "FIM1", 4) == 0) {
            auto message = TRY(decode_message(bytes));
            if (message.epoch != expected_epoch)
                return Error::from_string_literal("Stale application message epoch");
            return Frame { message.kind, message.success, message.epoch, message.request,
                message.payload, false, message.payload.size(), 0 };
        }

        if (bytes.size() < fragment_header_size || bytes.size() > frame_limit
            || __builtin_memcmp(bytes.data(), "FIM2", 4) || bytes[4] < 1 || bytes[4] > 4
            || bytes[5] > 1 || bytes[6] || bytes[7])
            return Error::from_string_literal("Invalid application fragment header");
        auto kind = static_cast<Kind>(bytes[4]);
        auto epoch = read64(bytes, 8);
        auto request = read32(bytes, 16);
        auto total = read32(bytes, 20);
        auto offset = read32(bytes, 24);
        auto length = read32(bytes, 28);
        if (epoch != expected_epoch || !request || (kind != Kind::Reply && bytes[5])
            || total <= wire_limit || total > logical_wire_limit || !length
            || length > fragment_payload_limit || length != bytes.size() - fragment_header_size
            || offset >= total || length > total - offset)
            return Error::from_string_literal("Invalid application fragment identity/length");
        return Frame { kind, bytes[5] != 0, epoch, request, bytes.slice(fragment_header_size), true,
            total, offset };
    }
}

ErrorOr<Fragment> FragmentEncoder::encode_next(
    Kind kind, bool success, u64 epoch, u32 request, ReadonlyBytes wire, size_t offset)
{
    if (wire.size() <= wire_limit) {
        if (offset)
            return Error::from_string_literal("Invalid single-frame application offset");
        return Fragment { TRY(encode_message(kind, success, epoch, request, wire)), wire.size(),
            true };
    }

    if (!epoch || !request || wire.size() > logical_wire_limit || offset >= wire.size()
        || to_underlying(kind) < 1 || to_underlying(kind) > 4 || (kind != Kind::Reply && success))
        return Error::from_string_literal("Invalid fragmented application message");
    size_t length = std::min(fragment_payload_limit, wire.size() - offset);
    auto bytes = ByteString::create_and_overwrite(fragment_header_size + length, [&](Bytes bytes) {
        __builtin_memset(bytes.data(), 0, fragment_header_size);
        __builtin_memcpy(bytes.data(), "FIM2", 4);
        bytes[4] = to_underlying(kind);
        bytes[5] = success;
        for (size_t i = 0; i < 8; ++i)
            bytes[8 + i] = static_cast<u8>(epoch >> (8 * i));
        write32(bytes, 16, request);
        write32(bytes, 20, wire.size());
        write32(bytes, 24, offset);
        write32(bytes, 28, length);
        __builtin_memcpy(bytes.data() + fragment_header_size, wire.data() + offset, length);
    });
    return Fragment { move(bytes), offset + length, offset + length == wire.size() };
}

Reassembler::Reassembler(Reassembler&& other)
{
    *this = move(other);
}

Reassembler& Reassembler::operator=(Reassembler&& other)
{
    if (this != &other) {
        reset();
        m_kind = other.m_kind;
        m_success = other.m_success;
        m_epoch = other.m_epoch;
        m_request = other.m_request;
        m_total = other.m_total;
        m_offset = other.m_offset;
        m_storage = move(other.m_storage);
        m_write = other.m_write;
        other.reset();
    }

    return *this;
}

ErrorOr<size_t> Reassembler::required_bytes(ReadonlyBytes bytes, u64 expected_epoch)
{
    return TRY(parse(bytes, expected_epoch)).total;
}

ErrorOr<Optional<OwnedMessage>> Reassembler::accept(ReadonlyBytes bytes, u64 expected_epoch)
{
    auto frame = TRY(parse(bytes, expected_epoch));
    if (!frame.fragmented) {
        if (active())
            return Error::from_string_literal("Application messages interleaved with fragments");
        return Optional<OwnedMessage> { OwnedMessage {
            frame.kind, frame.success, frame.epoch, frame.request, ByteString(frame.payload) } };
    }

    if (!active()) {
        if (frame.offset)
            return Error::from_string_literal("Application fragments must start at offset zero");
        // The caller has preflighted/reserved frame.total before this sole
        // allocation. Keep it unpublished while writing contiguous payloads.
        m_storage = AK::ByteStringImpl::create_uninitialized(frame.total, m_write);
        m_kind = frame.kind;
        m_success = frame.success;
        m_epoch = frame.epoch;
        m_request = frame.request;
        m_total = frame.total;
        m_offset = 0;
    } else if (frame.kind != m_kind || frame.success != m_success || frame.epoch != m_epoch
        || frame.request != m_request || frame.total != m_total)
        return Error::from_string_literal("Application fragment identity/total changed");
    if (frame.offset != m_offset)
        return Error::from_string_literal("Application fragments overlap, restart or have a gap");
    __builtin_memcpy(m_write + m_offset, frame.payload.data(), frame.payload.size());
    m_offset += frame.payload.size();
    if (m_offset != m_total)
        return Optional<OwnedMessage> {};
    OwnedMessage message { m_kind, m_success, m_epoch, m_request,
        ByteString(m_storage.release_nonnull()) };
    reset();
    return Optional<OwnedMessage> { move(message) };
}

void Reassembler::reset()
{
    m_storage = nullptr;
    m_write = nullptr;
    m_total = 0;
    m_offset = 0;
    m_epoch = 0;
    m_request = 0;
    m_kind = Kind::Ack;
    m_success = false;
}
}
