/** Credit-paced file service over the existing bounded native byte channel. No LibWeb or JS. */
#include "Server.h"
#include <AK/Array.h>
#include <AK/ScopeGuard.h>
#include <LibCore/EventLoop.h>

namespace Firefly::Protocols {
FileServer::FileServer(
    u64 epoch, NonnullRefPtr<FileRequests> files, NonnullRefPtr<Callbacks> callbacks)
    : m_files(move(files))
    , m_callbacks(move(callbacks))
    , m_epoch(epoch)
{
}

ErrorOr<NonnullRefPtr<FileServer>> FileServer::create(
    NonnullOwnPtr<IPC::Transport> transport, u64 epoch, Authorize authorize)
{
    if (!authorize)
        return Error::from_string_literal("File service requires native authorization");
    return create_async(move(transport), epoch,
        [authorize = move(authorize)](AuthorizationRequest request, Completion complete) {
            (void)complete(authorize(request.url, request.head));
        });
}

ErrorOr<NonnullRefPtr<FileServer>> FileServer::create_async(NonnullOwnPtr<IPC::Transport> transport,
    u64 epoch, AuthorizeAsync authorize, CancelAuthorization cancel)
{
    if (!authorize)
        return Error::from_string_literal("File service requires native authorization");
    auto files = TRY(FileRequests::create(epoch));
    auto server = adopt_ref(*new FileServer(
        epoch, move(files), adopt_ref(*new Callbacks(move(authorize), move(cancel)))));
    auto weak = server->make_weak_ptr();
    server->m_channel = TRY(Bytes::Channel::create(move(transport),
        {
            .on_data =
                [weak](ReadonlyBytes bytes) {
                    if (auto self = weak.strong_ref())
                        self->receive(bytes);
                },
            .on_writable =
                [weak] {
                    if (auto self = weak.strong_ref())
                        self->schedule();
                },
            .on_closed =
                [weak](auto) {
                    if (auto self = weak.strong_ref())
                        self->revoke();
                },
        }));
    server->m_timer = Core::Timer::create_repeating(AK::Duration::from_seconds(1), [weak] {
        if (auto self = weak.strong_ref())
            self->expire();
    });
    server->m_timer->start();
    return server;
}

FileServer::~FileServer()
{
    revoke();
}

bool FileServer::is_open() const
{
    return !m_revoked && m_channel && m_channel->is_open();
}

void FileServer::revoke()
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_revoked)
        return;
    m_revoked = true;
    revoke_weak_ptrs(); // Invalidate completions before destructor/cancellation callback reentry.
    if (m_timer)
        m_timer->stop();
    Array<u32, document_file_request_limit> cancelled;
    size_t count = 0;
    for (auto const& entry : m_jobs)
        if (entry.value.authorizing && !entry.value.cancelled)
            cancelled[count++] = entry.key;
    auto callbacks = m_callbacks;
    m_files->revoke();
    m_jobs.clear();
    if (m_channel)
        m_channel->close();
    // Retain the callback owner through reentry; all service state is terminal first.
    if (callbacks->cancel)
        for (size_t i = 0; i < count; ++i)
            callbacks->cancel(cancelled[i]);
}

void FileServer::receive(ReadonlyBytes bytes)
{
    VERIFY(m_thread == AK::ThreadID::current());
    auto decoded = decode_file_request(bytes);
    if (!is_open() || decoded.is_error() || decoded.value().epoch != m_epoch || m_admitting) {
        revoke();
        return;
    }

    auto const& packet = decoded.value();
    if (packet.kind == FileRequestKind::Get || packet.kind == FileRequestKind::Head) {
        // Consume all request IDs, including refused routes. An old reply cannot
        // later become a different file. Bound even jobs waiting to send refusal.
        if (packet.request <= m_last_request || m_jobs.size() >= document_file_request_limit) {
            revoke();
            return;
        }

        m_last_request = packet.request;
        m_admitting = true;
        ScopeGuard admission = [&] { m_admitting = false; };
        Job job;
        auto now = MonotonicTime::now();
        job.deadline = now + AK::Duration::from_seconds(30);
        auto admitted
            = m_files->admit(m_epoch, packet.request, packet.kind == FileRequestKind::Head, now);
        if (admitted.is_error()) {
            job.authorizing = false;
            job.cancelled = true;
        }

        if (m_jobs.try_set(packet.request, move(job)).is_error()) {
            revoke();
            return;
        }

        if (!admitted.is_error()) {
            auto id = packet.request;
            auto weak = make_weak_ptr();
            auto callbacks = m_callbacks;
            // Own URL storage before application code can retain or reenter the callback.
            AuthorizationRequest request { id,
                ByteString { reinterpret_cast<char const*>(packet.url.data()), packet.url.size() },
                packet.kind == FileRequestKind::Head };
            callbacks->start(
                move(request), [weak, id, thread = m_thread](ErrorOr<AuthorizedFile> result) {
                    VERIFY(thread == AK::ThreadID::current());
                    if (auto self = weak.strong_ref())
                        return self->authorize(id, move(result));
                    return false;
                });
        }
    } else {
        // Credit/cancel for a retired ID may already be in flight at completion.
        // Future IDs are never admitted implicitly.
        if (packet.request > m_last_request) {
            revoke();
            return;
        }

        auto found = m_jobs.find(packet.request);
        if (found != m_jobs.end()) {
            if (packet.kind == FileRequestKind::Cancel) {
                m_files->cancel(m_epoch, packet.request);
                bool authorizing = exchange(found->value.authorizing, false);
                found->value.cancelled = true;
                found->value.metadata = {};
                auto callbacks = m_callbacks;
                if (authorizing && callbacks->cancel)
                    callbacks->cancel(packet.request);
            } else if (!found->value.cancelled && !found->value.complete) {
                if (packet.credit > file_chunk_limit - found->value.credit) {
                    revoke();
                    return;
                }

                found->value.credit += packet.credit;
            }
        }
    }

    schedule();
}

bool FileServer::authorize(u32 request, ErrorOr<AuthorizedFile> result)
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (!is_open())
        return false;
    auto found = m_jobs.find(request);
    if (found == m_jobs.end() || !found->value.authorizing || found->value.cancelled)
        return false;
    auto& job = found->value;
    job.authorizing = false;
    auto now = MonotonicTime::now();
    if (now >= job.deadline) {
        revoke();
        return false;
    }

    if (result.is_error()) {
        m_files->cancel(m_epoch, request);
        job.cancelled = true;
    } else {
        auto file = result.release_value();
        auto metadata = m_files->authorize(m_epoch, request, file.path, file.headers, now);
        if (metadata.is_error()) {
            job.cancelled = true;
        } else {
            job.head_or_empty = metadata.value().head || !metadata.value().content_length;
            auto encoded = encode_file_response_head(metadata.value());
            if (encoded.is_error()) {
                m_files->cancel(m_epoch, request);
                job.cancelled = true;
            } else
                job.metadata = encoded.release_value().to_byte_buffer();
        }
    }

    schedule();
    return true;
}

void FileServer::schedule()
{
    if (!is_open() || m_scheduled || m_jobs.is_empty())
        return;
    m_scheduled = true;
    auto weak = make_weak_ptr();
    Core::deferred_invoke([weak] {
        if (auto self = weak.strong_ref())
            self->pump();
    });
}

bool FileServer::send(FilePacketKind kind, u32 id, u64 offset, ReadonlyBytes payload)
{
    auto encoded = encode_file_packet({ kind, m_epoch, id, offset, payload });
    if (encoded.is_error() || m_channel->send(encoded.value()).is_error()) {
        revoke();
        return false;
    }

    return true;
}

void FileServer::pump()
{
    VERIFY(m_thread == AK::ThreadID::current());
    m_scheduled = false;
    if (!is_open() || m_channel->pending_send_count() >= Bytes::max_pending)
        return; // Resume only on a real transport acknowledgement.
    Optional<u32> after, first;
    for (auto const& entry : m_jobs) {
        auto const& job = entry.value;
        if (job.authorizing && !job.cancelled)
            continue;
        if (!(job.cancelled || job.complete || !job.headers_sent || job.head_or_empty
                || job.credit))
            continue;
        if (!first.has_value() || entry.key < *first)
            first = entry.key;
        if (entry.key > m_last_pumped && (!after.has_value() || entry.key < *after))
            after = entry.key;
    }

    auto id = after.has_value() ? after : first;
    if (!id.has_value())
        return; // All producers are paused by the consumers' body credit.
    m_last_pumped = *id;
    auto& job = m_jobs.find(*id)->value;
    if (job.cancelled || job.complete) {
        auto kind = job.cancelled ? FilePacketKind::Cancel : FilePacketKind::Complete;
        auto offset = job.cancelled ? 0 : job.offset;
        if (!send(kind, *id, offset))
            return;
        m_jobs.remove(*id);
    } else if (!job.headers_sent) {
        if (!send(FilePacketKind::Metadata, *id, 0, job.metadata))
            return;
        job.headers_sent = true;
        job.metadata = {};
    } else {
        auto chunk = m_files->pull(m_epoch, *id,
            job.head_or_empty ? 1 : static_cast<size_t>(job.credit), MonotonicTime::now());
        if (chunk.is_error()) {
            m_files->cancel(m_epoch, *id);
            job.cancelled = true;
        } else {
            auto value = chunk.release_value();
            job.offset = value.offset + value.bytes.size();
            job.credit -= value.bytes.size();
            job.complete = value.complete;
            if (!value.bytes.is_empty()
                && !send(FilePacketKind::Chunk, *id, value.offset, value.bytes))
                return;
        }
    }

    schedule(); // One frame/chunk per task; round-robin across active requests.
}

void FileServer::expire()
{
    if (!is_open())
        return;
    // Include refused/finished jobs queued behind a stalled transport, not just
    // open files. Credit never extends a request's fixed lifetime. Disconnect
    // settles all consumers if any job cannot finish within this channel budget.
    auto now = MonotonicTime::now();
    for (auto const& entry : m_jobs) {
        if (now >= entry.value.deadline) {
            revoke();
            return;
        }
    }

    auto expired = m_files->expire(now);
    if (expired.is_error()) {
        revoke();
        return;
    }

    for (auto id : expired.value()) {
        auto found = m_jobs.find(id);
        if (found != m_jobs.end()) {
            found->value.cancelled = true;
            found->value.metadata = {};
        }
    }

    schedule();
}
}
