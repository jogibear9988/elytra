/** Native request/credit channel scoped to one document or one explicit UI navigation. */
#pragma once
#include "Navigation.h"
#include "Response.h"
#include "native/ipc/bytes/Channel.h"
#include "native/platform/QueueSnapshots.h"
#include "native/protocol/files/RequestPacket.h"
#include <LibCore/Timer.h>
#include <LibWeb/DOM/Document.h>

namespace Firefly::ProtocolFetch {
class Client final : public RefCounted<Client>, public Weakable<Client> {
public:
    // Native caller admits this transport/epoch for this exact active document.
    // Creating a client neither installs a provider nor exports any page API.
    static ErrorOr<NonnullRefPtr<Client>> create(
        NonnullOwnPtr<IPC::Transport>, u64 epoch, Web::DOM::Document&);
    // Separate one-shot scope: the native navigable must already carry this
    // navigation ID. This does not weaken document-client request admission.

    static ErrorOr<NonnullRefPtr<Client>> create_navigation(NonnullOwnPtr<IPC::Transport>,
        u64 epoch, Web::HTML::LocalNavigable&, URL::URL expected_url, Utf16String navigation_id);

    ~Client();

    ErrorOr<NonnullRefPtr<Response>> fetch(Web::Fetch::Infrastructure::FetchParams const&);

    void revoke();

    bool is_revoked() const
    {
        return m_revoked;
    }

    size_t pending() const
    {
        return m_jobs.size();
    }

private:
    struct Job {
        RefPtr<Response> response;
        ByteString url;
        MonotonicTime deadline;
        u64 credit { 0 };
        bool head { false };
        bool sent { false };
        bool cancelled { false };
        bool cancel_sent { false };
    };

    Client(u64, Web::DOM::Document&);

    ErrorOr<void> initialize(NonnullOwnPtr<IPC::Transport>);

    bool scope_is_active() const;

    void receive(ReadonlyBytes);

    void credit(u32, u64);

    void cancel(u32);

    void schedule();

    void pump();

    void sweep();

    void remove_job(u32);

    static void wake_clients();

    RefPtr<Bytes::Channel> m_channel;
    GC::Root<Web::DOM::Document> m_document;
    OwnPtr<Navigation> m_navigation;
    HashMap<u32, Job> m_jobs;
    RefPtr<Core::Timer> m_timer;
    URL::Origin const m_origin;
    u64 const m_epoch;
    u32 m_next { 0 };
    u32 m_last_pumped { 0 };
    QueueSnapshotClock m_diagnostics;
    size_t m_active { 0 };
    size_t m_waiting_url_bytes { 0 };
    bool m_revoked { false };
    bool m_scheduled { false };
    AK::ThreadID m_thread { AK::ThreadID::current() };
};
}
