/** Exercise actual descriptor ownership, quotas, deadlines and stale callbacks without GUI/VM. */
#include "FileRequestsProbe.h"
#include "native/protocol/files/FileRequests.h"
#include <AK/Array.h>
#include <AK/WeakPtr.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

using namespace Firefly::Protocols;

static ErrorOr<void> check(bool value, StringView message)
{
    if (!value)
        return Error::from_string_view(message);
    return {};
}

ErrorOr<void> check_file_requests(StringView path)
{
    auto now = MonotonicTime::now();
    auto later = now + AK::Duration::from_seconds(30);
    TRY(check(FileRequests::create(0).is_error(), "Zero epoch admitted"sv));
    {
        auto owner = TRY(FileRequests::create(101));
        TRY(check(
            owner->begin(100, 1, path, false, {}, now).is_error(), "Foreign epoch admitted"sv));
        TRY(check(
            owner->begin(101, 0, path, false, {}, now).is_error(), "Zero request admitted"sv));
        auto metadata = TRY(owner->begin(101, 1, path, false, {}, now));
        TRY(check(metadata.request == 1 && owner->pending() == 1, "Request not owned"sv));
        TRY(check(
            owner->begin(101, 1, path, false, {}, now).is_error(), "Duplicate request admitted"sv));
        TRY(check(owner->pull(100, 1, 17, now).is_error() && !owner->cancel(100, 1)
                && owner->pending() == 1,
            "Foreign epoch changed active request"sv));
        auto first = TRY(owner->pull(101, 1, 17, now));
        auto second = TRY(owner->pull(101, 1, 23, now));
        TRY(check(first.offset == 0 && first.bytes.size() == 17 && !first.complete
                && second.offset == 17 && second.bytes.size() == 23,
            "Chunk offsets changed"sv));
        TRY(check(owner->pull(101, 1, 0, now).is_error()
                && owner->pull(101, 1, file_chunk_limit + 1, now).is_error()
                && owner->pending() == 1,
            "Invalid credit consumed a job"sv));
        TRY(owner->begin(101, 2, path, true, {}, now));
        auto head = TRY(owner->pull(101, 2, file_chunk_limit, now));
        TRY(check(head.complete && head.bytes.is_empty() && head.offset == 0
                && owner->pending() == 1 && !owner->cancel(101, 2),
            "HEAD ownership did not finish"sv));
        TRY(check(owner->begin(101, 2, path, true, {}, now).is_error(), "Completed ID reused"sv));
        auto almost = later - AK::Duration::from_milliseconds(1);
        TRY(owner->pull(101, 1, 1, almost));
        TRY(check(TRY(owner->expire(almost)).is_empty(), "Request expired early"sv));
        auto expired = TRY(owner->expire(later));
        TRY(check(expired.size() == 1 && expired[0] == 1 && !owner->pending()
                && owner->pull(101, 1, 1, later).is_error(),
            "Fixed deadline/expiry failed"sv));
        auto missing = ByteString::formatted("{}.missing", path);
        TRY(check(owner->begin(101, 3, missing, false, {}, now).is_error()
                && owner->begin(101, 3, path, false, {}, now).is_error(),
            "Failed open ID reused"sv));
        Array<HTTP::Header, 1> invalid { { { "Content-Length", "0" } } };
        TRY(check(owner->begin(101, 4, path, false, invalid, now).is_error() && !owner->pending(),
            "Metadata failure retained job/credit"sv));
        TRY(owner->begin(101, 5, path, false, {}, now));
        TRY(check(owner->cancel(101, 5) && !owner->cancel(101, 5) && !owner->pending(),
            "Cancel not terminal"sv));
        TRY(owner->begin(101, 6, path, false, {}, now));
        auto timed = owner->pull(101, 6, 1, later);
        TRY(check(timed.is_error() && timed.error().is_errno() && timed.error().code() == ETIMEDOUT
                && !owner->pending(),
            "Pull did not expire request"sv));
        TRY(owner->begin(101, 7, path, false, {}, now));
        owner->revoke();
        owner->revoke();
        TRY(check(owner->is_revoked() && !owner->pending()
                && owner->begin(101, 8, path, false, {}, now).is_error()
                && owner->pull(101, 7, 1, now).is_error(),
            "Revoked owner revived"sv));
    }

    {
        WeakPtr<FileRequests> stale;
        {
            auto owner = TRY(FileRequests::create(111));
            stale = owner->make_weak_ptr();
            TRY(owner->begin(111, 0xffffffffu, path, true, {}, now));
            TRY(owner->pull(111, 0xffffffffu, 1, now));
            TRY(check(
                owner->begin(111, 1, path, false, {}, now).is_error(), "Request ID wrapped"sv));
            owner->revoke();
            auto callback_owner = stale.strong_ref();
            TRY(check(callback_owner && callback_owner->pull(111, 0xffffffffu, 1, now).is_error(),
                "Late callback revived revoked owner"sv));
        }

        TRY(check(!stale.strong_ref(), "Weak callback retained destroyed document owner"sv));
        auto next = TRY(FileRequests::create(112));
        TRY(next->begin(112, 1, path, true, {}, now));
        TRY(check(next->pull(111, 1, 1, now).is_error(), "Old epoch addressed new document"sv));
    }

    {
        auto owner = TRY(FileRequests::create(113));
        auto bytes = TRY(ByteBuffer::create_uninitialized(6000));
        __builtin_memset(bytes.data(), 1, bytes.size());
        Array<HTTP::Header, 1> expansion { { { "X-Escaped",
            ByteString(
                StringView { reinterpret_cast<char const*>(bytes.data()), bytes.size() }) } } };
        TRY(check(owner->begin(113, 1, path, false, expansion, now).is_error() && !owner->pending(),
            "Encoded metadata overflow retained request"sv));
        TRY(owner->begin(113, 2, path, true, {}, now));
    }

    {
        auto owner = TRY(FileRequests::create(102));
        for (u32 id = 1; id <= document_file_request_limit; ++id)
            TRY(owner->begin(102, id, path, true, {}, now));
        TRY(check(owner->begin(102, 9, path, true, {}, now).is_error() && owner->pending() == 8,
            "Document request budget bypass"sv));
        TRY(check(owner->cancel(102, 1), "Budget cancel failed"sv));
        TRY(check(
            owner->begin(102, 9, path, true, {}, now).is_error(), "Budget failure ID reused"sv));
        TRY(owner->begin(102, 10, path, true, {}, now));
        // Destruction must release all eight leases, without explicit revoke.
    }

    {
        Vector<NonnullRefPtr<FileRequests>> owners;
        for (size_t i = 0; i < 8; ++i) {
            auto owner = TRY(FileRequests::create(i + 1));
            for (u32 id = 1; id <= 8; ++id)
                TRY(owner->begin(i + 1, id, path, true, {}, now));
            owners.append(move(owner));
        }

        auto excess = TRY(FileRequests::create(99));
        TRY(check(excess->begin(99, 1, path, false, {}, now).is_error(),
            "Process request budget bypass"sv));
        owners[0]->revoke();
        TRY(excess->begin(99, 2, path, false, {}, now));
    }
    // Different native owner threads must still share exactly 64 live credits.
    {
        std::mutex lock;
        std::condition_variable wake;
        size_t arrived = 0;
        bool release = false;
        std::atomic<size_t> admitted { 0 };
        std::atomic<bool> bad { false };
        std::vector<std::thread> workers;
        for (u64 epoch = 1; epoch <= 12; ++epoch)
            workers.emplace_back([&, epoch] {
                auto owner = FileRequests::create(epoch);
                if (owner.is_error())
                    bad = true;
                else
                    for (u32 id = 1; id <= 8; ++id) {
                        auto result
                            = owner.value()->begin(epoch, id, path, true, {}, MonotonicTime::now());
                        if (!result.is_error())
                            ++admitted;
                        else if (!result.error().is_errno() || result.error().code() != EAGAIN)
                            bad = true;
                    }

                std::unique_lock guard(lock);
                ++arrived;
                wake.notify_all();
                wake.wait(guard, [&] { return release; });
            });
        bool all_ready;
        {
            std::unique_lock guard(lock);
            all_ready
                = wake.wait_for(guard, std::chrono::seconds(10), [&] { return arrived == 12; });
            release = true;
        }

        wake.notify_all();
        for (auto& worker : workers)
            worker.join();
        TRY(check(
            all_ready && !bad && admitted == 64, "Concurrent process credit accounting failed"sv));
    }

    {
        auto owner = TRY(FileRequests::create(103));
        auto metadata = TRY(owner->begin(103, 1, path, false, {}, now));
        u64 position = 0;
        for (;;) {
            auto chunk = TRY(owner->pull(103, 1, 257, now));
            TRY(check(chunk.offset == position, "Interleaved pull changed offset"sv));
            position += chunk.bytes.size();
            if (chunk.complete)
                break;
        }

        TRY(check(position == metadata.content_length && !owner->pending()
                && owner->pull(103, 1, 1, now).is_error(),
            "Completion retained request"sv));
        TRY(owner->begin(103, 2, path, false, {}, now));
        auto writer = TRY(Core::File::open(path, Core::File::OpenMode::Write));
        writer->close();
        TRY(check(owner->pull(103, 2, 1, now).is_error() && !owner->pending(),
            "IO failure retained job"sv));
    }

    outln("PASS actual file request epochs/IDs/offsets, HEAD/completion/cancel/error/destructor "
          "cleanup, fixed deadlines, per-document and concurrent process quotas");
    return {};
}
