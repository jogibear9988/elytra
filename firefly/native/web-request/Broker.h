/** UI-owned authenticated helper channels. No application callbacks or VM roots cross IPC. */
#pragma once
#include "Packet.h"
#include "Policy.h"
#include "native/ipc/bytes/Channel.h"
#include "native/platform/QueueSnapshots.h"
#include <AK/HashMap.h>
#include <AK/WeakPtr.h>
#include <LibCore/Timer.h>
#include <LibWebView/WebContentClient.h>

namespace WebView {
class WebContentPage;
}

namespace Firefly::WebRequest {
class Broker final : public RefCounted<Broker>, public Weakable<Broker> {
public:
    using Authenticate = Function<Optional<JsonObject>(WebView::WebContentClient&, Packet const&)>;
    using Send = Function<void(int contents, ByteString metadata)>;
    Broker(Authenticate, Send);

    ~Broker();

    bool configure(u32 epoch, bool before, bool headers);

    ErrorOr<void> bind(WebView::WebContentPage&);

    bool complete(u32 token, u32 epoch, JsonObject);

    bool current(u32 token, u32 epoch);

    void sweep();

    void stop();

private:
    struct Client {
        NonnullRefPtr<WebView::WebContentClient> owner;
        RefPtr<Bytes::Channel> channel;
        u64 cookie_high;
        u64 cookie_low;
        bool ready { false };
        u32 last_request { 0 };
        HashMap<u32, u32> logical_ids;
        MonotonicTime deadline;
    };

    struct Pending {
        u64 client;
        Packet packet;
        MonotonicTime deadline;
        size_t bytes;
    };

    bool send(u64 client, Packet const&);

    void receive(u64 client, ReadonlyBytes);

    void retire(u64 client);

    void deny(u32 token);

    Authenticate m_authenticate;
    Send m_send;
    HashMap<u64, Client> m_clients;
    HashMap<u32, Pending> m_pending;
    u64 m_next_client { 0 };
    u32 m_next_token { 0 };
    u32 m_next_public_id { 1 };
    u32 m_epoch { 0 };
    bool m_before { false };
    bool m_headers { false };
    bool m_stopped { false };
    QueueSnapshotClock m_diagnostics;
    size_t m_bytes { 0 };
};
}
