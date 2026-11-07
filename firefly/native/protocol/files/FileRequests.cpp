/** Bound active jobs across documents and release both files and credit on every terminal path. */
#include "FileRequests.h"
#include <AK/ScopeGuard.h>
#include <atomic>

namespace Firefly::Protocols {
namespace {
    std::atomic<size_t> process_pending { 0 };
}

ErrorOr<FileRequests::Lease> FileRequests::Lease::reserve()
{
    auto count = process_pending.load(std::memory_order_relaxed);
    while (count < process_file_request_limit) {
        if (process_pending.compare_exchange_weak(count, count + 1, std::memory_order_acq_rel))
            return Lease {};
    }

    return Error::from_errno(EAGAIN);
}

FileRequests::Lease::~Lease()
{
    if (m_held)
        process_pending.fetch_sub(1, std::memory_order_acq_rel);
}

FileRequests::Lease& FileRequests::Lease::operator=(Lease&& other)
{
    if (this != &other) {
        if (m_held)
            process_pending.fetch_sub(1, std::memory_order_acq_rel);
        m_held = exchange(other.m_held, false);
    }

    return *this;
}

ErrorOr<NonnullRefPtr<FileRequests>> FileRequests::create(u64 epoch)
{
    if (!epoch)
        return Error::from_errno(EINVAL);
    return adopt_nonnull_ref_or_enomem(new (nothrow) FileRequests(epoch));
}

FileRequests::~FileRequests()
{
    revoke();
}

ErrorOr<void> FileRequests::check(u64 epoch) const
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_revoked || epoch != m_epoch)
        return Error::from_errno(ECANCELED);
    return {};
}

ErrorOr<void> FileRequests::admit(u64 epoch, u32 request, bool head, MonotonicTime now)
{
    TRY(check(epoch));
    // Consume even a refused attempt. Late callbacks can never reuse this identity.
    if (!request || request <= m_last_request)
        return Error::from_errno(EINVAL);
    m_last_request = request;
    if (m_requests.size() >= document_file_request_limit)
        return Error::from_errno(EAGAIN);
    auto lease = TRY(Lease::reserve());
    TRY(m_requests.try_set(
        request, Request { move(lease), nullptr, now + AK::Duration::from_seconds(30), head }));
    return {};
}

ErrorOr<ResponseHead> FileRequests::authorize(
    u64 epoch, u32 request, StringView path, ReadonlySpan<HTTP::Header> headers, MonotonicTime now)
{
    TRY(check(epoch));
    auto found = m_requests.find(request);
    if (found == m_requests.end())
        return Error::from_errno(ENOENT);
    if (found->value.body)
        return Error::from_errno(EALREADY);
    ArmedScopeGuard failure = [&] { m_requests.remove(request); };
    if (now >= found->value.deadline)
        return Error::from_errno(ETIMEDOUT);
    auto body = TRY(FileBody::open(path, found->value.head));
    auto metadata = TRY(
        make_file_response_head(request, body->content_length(), found->value.head, path, headers));
    // Escaping may exceed the wire budget despite valid raw HTTP header bytes.
    TRY(encode_file_response_head(metadata));
    found->value.body = move(body);
    failure.disarm();
    return metadata;
}

ErrorOr<ResponseHead> FileRequests::begin(u64 epoch, u32 request, StringView path, bool head,
    ReadonlySpan<HTTP::Header> headers, MonotonicTime now)
{
    TRY(admit(epoch, request, head, now));
    return authorize(epoch, request, path, headers, now);
}

size_t FileRequests::files() const
{
    size_t count = 0;
    for (auto const& entry : m_requests)
        if (entry.value.body)
            ++count;
    return count;
}

ErrorOr<FileChunk> FileRequests::pull(u64 epoch, u32 request, size_t maximum, MonotonicTime now)
{
    TRY(check(epoch));
    auto found = m_requests.find(request);
    if (found == m_requests.end())
        return Error::from_errno(ENOENT);
    auto& body = found->value.body;
    if (now >= found->value.deadline) {
        m_requests.remove(found);
        return Error::from_errno(ETIMEDOUT);
    }

    if (!maximum || maximum > file_chunk_limit)
        return Error::from_errno(EINVAL);
    if (!body)
        return Error::from_errno(EAGAIN);
    auto offset = body->transferred();
    auto bytes = body->read(maximum);
    if (bytes.is_error()) {
        m_requests.remove(found);
        return bytes.release_error();
    }

    bool complete = body->state() == FileBody::State::Complete;
    if (complete)
        m_requests.remove(found);
    return FileChunk { offset, bytes.release_value(), complete };
}

bool FileRequests::cancel(u64 epoch, u32 request)
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_revoked || epoch != m_epoch)
        return false;
    return m_requests.remove(request);
}

ErrorOr<Vector<u32>> FileRequests::expire(MonotonicTime now)
{
    VERIFY(m_thread == AK::ThreadID::current());
    Vector<u32> expired;
    // Allocate before modifying ownership, so failure cannot lose settlement IDs.
    TRY(expired.try_ensure_capacity(m_requests.size()));
    for (auto const& entry : m_requests)
        if (now >= entry.value.deadline)
            expired.unchecked_append(entry.key);
    for (auto id : expired)
        m_requests.remove(id);
    return expired;
}

void FileRequests::revoke()
{
    VERIFY(m_thread == AK::ThreadID::current());
    m_revoked = true;
    m_requests.clear();
}
}
