/** Pending native grants become clients only for the exact newly activated document. */
#pragma once
#include "loading/protocol/Client.h"
#include <LibIPC/FireflyBootstrap.h>

namespace WebContent {
class ConnectionFromClient;
}

namespace Firefly::Resources {
class Documents {
public:
    ErrorOr<void> receive(
        WebContent::ConnectionFromClient&, IPC::TransportHandle, Bytes::DocumentBinding);

    ErrorOr<NonnullRefPtr<ProtocolFetch::Response>> fetch(
        Web::Fetch::Infrastructure::FetchParams const&);

    void sweep();

    void revoke();

private:
    struct Record {
        Bytes::DocumentBinding binding;
        URL::URL url;
        OwnPtr<IPC::Transport> transport;
        RefPtr<ProtocolFetch::Client> client;
        GC::Root<Web::DOM::Document> document;
        MonotonicTime admitted;
    };

    HashMap<Web::HTML::CrossProcessId, Record> m_records;
};
}
