/** One native-authorized resource channel; checked files stay on the main side. */
#pragma once
#include "FilePacket.h"
#include "FileRequests.h"
#include "RequestPacket.h"
#include "native/ipc/bytes/Channel.h"
#include <LibCore/Timer.h>

namespace Firefly::Protocols {
struct AuthorizedFile {
    ByteString path;
    Vector<HTTP::Header> headers;
};

class FileServer final : public RefCounted<FileServer>, public Weakable<FileServer> {
public:
    // The trusted owner binds this transport/epoch to one document before create.
    // The callback must authorize the URL for that document and return its path.
    using Authorize = Function<ErrorOr<AuthorizedFile>(StringView url, bool head)>;
    static ErrorOr<NonnullRefPtr<FileServer>> create(
        NonnullOwnPtr<IPC::Transport>, u64 epoch, Authorize);

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
        return m_files->pending();
    }

private:
    struct Job {
        ByteBuffer metadata;
        u64 credit { 0 };
        u64 offset { 0 };
        bool headers_sent { false };
        bool complete { false };
        bool cancelled { false };
        bool head_or_empty { false };
        MonotonicTime deadline;
    };

    FileServer(u64 epoch, NonnullRefPtr<FileRequests>, Authorize);

    void receive(ReadonlyBytes);

    void schedule();

    void pump();

    void expire();

    bool send(FilePacketKind, u32, u64 offset, ReadonlyBytes = {});

    RefPtr<Bytes::Channel> m_channel;
    NonnullRefPtr<FileRequests> m_files;
    Authorize m_authorize;
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
