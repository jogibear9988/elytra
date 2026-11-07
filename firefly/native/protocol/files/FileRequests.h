/** One admitted document's checked file jobs. The native caller owns admission and clock. */
#pragma once
#include "FileBody.h"
#include "ResponseHead.h"
#include <AK/HashMap.h>
#include <AK/RefCounted.h>
#include <AK/ThreadID.h>
#include <AK/Time.h>
#include <AK/Weakable.h>

namespace Firefly::Protocols {
inline constexpr size_t document_file_request_limit = 8;
inline constexpr size_t process_file_request_limit = 64;

struct FileChunk {
    u64 offset;
    ByteBuffer bytes;
    bool complete;
};

class FileRequests final : public RefCounted<FileRequests>, public Weakable<FileRequests> {
public:
    // Epoch comes from the trusted native document owner, never a renderer field.
    static ErrorOr<NonnullRefPtr<FileRequests>> create(u64 epoch);

    ~FileRequests();

    ErrorOr<ResponseHead> begin(u64 epoch, u32 request, StringView authorized_path, bool head,
        ReadonlySpan<HTTP::Header>, MonotonicTime now);

    ErrorOr<FileChunk> pull(u64 epoch, u32 request, size_t maximum, MonotonicTime now);

    bool cancel(u64 epoch, u32 request);

    // Caller schedules sweeping on its native event loop and settles these IDs.
    ErrorOr<Vector<u32>> expire(MonotonicTime now);

    // Permanent: old callbacks cannot revive an owner after navigation/disconnect.
    void revoke();

    size_t pending() const
    {
        return m_requests.size();
    }

    bool is_revoked() const
    {
        return m_revoked;
    }

private:
    class Lease {
        AK_MAKE_NONCOPYABLE(Lease);

    public:
        static ErrorOr<Lease> reserve();

        Lease(Lease&& other)
            : m_held(exchange(other.m_held, false))
        {
        }

        Lease& operator=(Lease&&);

        ~Lease();

    private:
        Lease() = default;
        bool m_held { true };
    };

    struct Request {
        // Reverse destruction order closes the file before returning its credit.
        Lease lease;
        NonnullOwnPtr<FileBody> body;
        MonotonicTime deadline;
    };

    explicit FileRequests(u64 epoch)
        : m_epoch(epoch)
    {
    }

    ErrorOr<void> check(u64 epoch) const;

    HashMap<u32, Request> m_requests;
    u64 const m_epoch;
    u32 m_last_request { 0 };
    bool m_revoked { false };
    AK::ThreadID m_thread { AK::ThreadID::current() };
};
}
