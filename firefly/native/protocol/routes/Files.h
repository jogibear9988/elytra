/** Default-session routes and trusted native navigation/document grants, owned by the main host. */
#pragma once
#include "native/protocol/files/Server.h"
#include <AK/JsonValue.h>
#include <LibWebCommon/HTML/NavigationPopulationRequest.h>

namespace WebView {
class WebContentPage;
}

namespace Firefly::Protocols {
class FileRoutes final : public RefCounted<FileRoutes>, public Weakable<FileRoutes> {
public:
    using Root = Web::HTML::CrossProcessId;
    using OwnsRoot = Function<bool(Root)>;
    using Send = Function<void(u64 token, ByteString type, ByteString metadata)>;
    using Failure = Function<void(Root, Utf16String const& navigation, URL::URL const&)>;

    FileRoutes(OwnsRoot, Send, Failure);

    ~FileRoutes();

    ErrorOr<Optional<u64>> register_route(StringView scheme);

    bool unregister_route(StringView scheme);

    bool has_route(StringView scheme) const;

    bool reply(u64 token, ErrorOr<AuthorizedFile>);

    bool is_pending(u64 token) const;

    bool mark_delivered(u64 token);

    ErrorOr<void> navigation(
        WebView::WebContentPage&, Web::HTML::NavigationPopulationRequest const&);

    ErrorOr<void> document(WebView::WebContentPage&, Web::HTML::NavigationPopulationRequest const&,
        Web::HTML::NavigationPopulationResult const&);

    void sweep();

    void stop();

private:
    struct Channel {
        Root root;
        Utf16String navigation;
        URL::URL url;
        u64 generation;
        bool document;
        NonnullRefPtr<FileServer> server;
    };

    struct Pending {
        u64 channel;
        u32 request;
        FileServer::Completion complete;
        bool delivered { false };
    };

    ErrorOr<void> grant(WebView::WebContentPage&, Web::HTML::NavigationPopulationRequest const&,
        Optional<Root> document);

    void request(u64 channel, FileServer::AuthorizationRequest, FileServer::Completion);

    void cancel(u64 channel, u32 request);

    void failed(u64 channel);

    void retire(u64 channel);

    HashMap<ByteString, u64> m_routes;
    HashMap<u64, Channel> m_channels;
    HashMap<u64, Pending> m_pending;
    OwnsRoot m_owns;
    Send m_send;
    Failure m_failure;
    u64 m_next_generation { 0 }, m_next_channel { 0 }, m_next_token { 0 };
    bool m_stopped { false };
};
}
