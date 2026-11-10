/** Native test authorizer for a fixed bundle; renderer URLs never become filesystem paths. */
#pragma once
#include "native/protocol/files/Server.h"
#include <LibWebView/WebContentPage.h>

namespace Firefly {
class DocumentResources final : public RefCounted<DocumentResources>,
                                public Weakable<DocumentResources> {
public:
    DocumentResources(ByteString fixture, ByteString mode);

    ~DocumentResources();

    ErrorOr<void> prepare(WebView::WebContentPage&, Web::HTML::NavigationPopulationRequest const&,
        Web::HTML::NavigationPopulationResult const&);

    bool ready(bool away) const;

    bool failed() const
    {
        return m_failed;
    }

    size_t requests() const
    {
        return m_requests;
    }

    void revoke();

private:
    ErrorOr<Protocols::AuthorizedFile> authorize(StringView url);

    void send(WebView::WebContentPage&, Web::HTML::NavigationPopulationRequest const&,
        Web::HTML::CrossProcessId document, IPC::TransportHandle, u64 epoch);

    ByteString m_directory;
    ByteString m_mode;
    RefPtr<Protocols::FileServer> m_server;
    RefPtr<Protocols::FileServer> m_retired_server;
    RefPtr<Bytes::Channel> m_duplicate;
    size_t m_requests { 0 };
    size_t m_refresh_requests { 0 };
    u32 m_seen { 0 };
    bool m_duplicate_closed { false };
    bool m_failed { false };
};
}
