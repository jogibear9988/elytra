/** One native-authorized resource channel; checked files stay on the main side. */
#pragma once
#include "FilePacket.h"
#include "FileRequests.h"
#include "RequestPacket.h"
#include "StreamResponseHead.h"
#include "native/ipc/bytes/Channel.h"
#include <AK/Variant.h>
#include <LibCore/Timer.h>

namespace Firefly::Protocols {
struct AuthorizedFile {
    ByteString path;
    Vector<HTTP::Header> headers;
};

// Application bodies use the same bounded native channel as descriptor-backed
// files, but production is asynchronous and happens only on consumer credit.
struct StreamChunk {
    ByteBuffer bytes;
    bool complete { false };
};

using StreamCompletion = Function<bool(ErrorOr<StreamChunk>)>;

struct AuthorizedStream {
    StreamResponseHead head;
    Function<void(size_t maximum, StreamCompletion)> pull;
    Function<void()> cancel;
};

class FileServer final : public RefCounted<FileServer>, public Weakable<FileServer> {
public:
    // The trusted owner binds this transport/epoch to one document before create.
    // The callback must authorize the URL for that document and return its path.
    using Authorize = Function<ErrorOr<AuthorizedFile>(StringView url, bool head)>;
    static ErrorOr<NonnullRefPtr<FileServer>> create(
        NonnullOwnPtr<IPC::Transport>, u64 epoch, Authorize);

    struct AuthorizationRequest {
        u32 id;
        ByteString url;
        bool head;
    };

    // Completion is VM-independent, owner-thread only, and safe after cancellation/destruction.
    // It returns false for a duplicate/stale result and never reopens such a file.
    using Completion = Function<bool(ErrorOr<AuthorizedFile>)>;
    using AuthorizeAsync = Function<void(AuthorizationRequest, Completion)>;
    using CancelAuthorization = Function<void(u32)>;
    using AuthorizationFailed = Function<void(u32)>;
    static ErrorOr<NonnullRefPtr<FileServer>> create_async(NonnullOwnPtr<IPC::Transport>, u64 epoch,
        AuthorizeAsync, CancelAuthorization = {}, AuthorizationFailed = {});

    using AuthorizedResponse = Variant<AuthorizedFile, AuthorizedStream>;
    using ResponseCompletion = Function<bool(ErrorOr<AuthorizedResponse>)>;
    using AuthorizeResponseAsync = Function<void(AuthorizationRequest, ResponseCompletion)>;
    static ErrorOr<NonnullRefPtr<FileServer>> create_responses(NonnullOwnPtr<IPC::Transport>,
        u64 epoch, AuthorizeResponseAsync, CancelAuthorization = {}, AuthorizationFailed = {});

    ~FileServer();

    void revoke();

    bool is_open() const;

    bool is_revoked() const
    {
        return m_revoked;
    }

    size_t pending() const
    {
        return m_jobs.size();
    }

    size_t files() const
    {
        return m_files->files();
    }

private:
    struct Producer : RefCounted<Producer> {
        explicit Producer(AuthorizedStream response)
            : response(move(response))
        {
        }

        ~Producer()
        {
            if (response.cancel)
                response.cancel();
        }

        AuthorizedStream response;
    };

    struct Job {
        ByteBuffer metadata;
        ByteBuffer ready_chunk;
        RefPtr<Producer> producer;
        Optional<u64> length;
        u64 credit { 0 };
        u64 offset { 0 };
        u64 pull_generation { 0 };
        size_t pull_maximum { 0 };
        bool pulling { false };
        bool head { false };
        bool authorizing { true };
        bool headers_sent { false };
        bool complete { false };
        bool cancelled { false };
        bool head_or_empty { false };
        MonotonicTime deadline;
    };

    struct Callbacks : RefCounted<Callbacks> {
        Callbacks(
            AuthorizeResponseAsync start, CancelAuthorization cancel, AuthorizationFailed failed)
            : start(move(start))
            , cancel(move(cancel))
            , failed(move(failed))
        {
        }

        AuthorizeResponseAsync start;
        CancelAuthorization cancel;
        AuthorizationFailed failed;
    };

    FileServer(u64 epoch, NonnullRefPtr<FileRequests>, NonnullRefPtr<Callbacks>);

    bool authorize(u32 request, ErrorOr<AuthorizedResponse>);

    bool produced(u32 request, u64 generation, ErrorOr<StreamChunk>);

    void receive(ReadonlyBytes);

    void schedule();

    void pump();

    void expire();

    bool send(FilePacketKind, u32, u64 offset, ReadonlyBytes = {});

    RefPtr<Bytes::Channel> m_channel;
    NonnullRefPtr<FileRequests> m_files;
    NonnullRefPtr<Callbacks> m_callbacks;
    HashMap<u32, Job> m_jobs;
    RefPtr<Core::Timer> m_timer;
    u64 const m_epoch;
    u32 m_last_request { 0 };
    u32 m_last_pumped { 0 };
    bool m_revoked { false };
    bool m_scheduled { false };
    bool m_admitting { false };
    AK::ThreadID m_thread { AK::ThreadID::current() };
};
}
