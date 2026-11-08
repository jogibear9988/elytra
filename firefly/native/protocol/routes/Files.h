/** Default-session routes and trusted native navigation/document grants, owned by the main host. */
#pragma once
#include "native/protocol/files/Server.h"
#include <AK/HashTable.h>
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

    using PolicyChanged = Function<void(u64 generation, bool active)>;

    FileRoutes(OwnsRoot, Send, Failure, PolicyChanged = {});

    ~FileRoutes();

    ErrorOr<Optional<u64>> register_route(StringView scheme, bool modern = false);

    bool unregister_route(StringView scheme);

    bool has_route(StringView scheme) const;

    bool intercepts_file() const;

    u64 file_policy_generation() const
    {
        return m_file_policy_generation;
    }

    ErrorOr<void> refresh_document(WebView::WebContentPage&);

    bool reply(u64 token, ErrorOr<AuthorizedFile>);

    bool reply_response(u64 token, ErrorOr<StreamResponseHead>);

    bool reply_chunk(u64 token, ReadonlyBytes, bool done);

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
        bool document;
        Optional<Root> document_id;
        NonnullRefPtr<FileServer> server;
        HashTable<ByteString> targets;
    };

    struct Route {
        u64 generation;
        bool modern;
    };

    struct Pending {
        u64 channel;
        u32 request;
        FileServer::ResponseCompletion complete;
        u64 generation;
        bool modern;
        bool delivered { false };
        bool streaming { false };
        size_t credit { 0 };
        StreamCompletion pull;
    };

    ErrorOr<void> grant(WebView::WebContentPage&, Web::HTML::NavigationPopulationRequest const&,
        Optional<Root> document);

    ErrorOr<void> grant(WebView::WebContentPage&, Root, Utf16String const& navigation,
        URL::URL const&, Optional<Root> document);

    void request(u64 channel, FileServer::AuthorizationRequest, FileServer::ResponseCompletion);

    void pull(u64 token, size_t maximum, StreamCompletion);

    void finish(u64 token, bool notify);

    void cancel(u64 channel, u32 request);

    void failed(u64 channel);

    void retire(u64 channel);

    HashMap<ByteString, Route> m_routes;
    HashMap<u64, Channel> m_channels;
    HashMap<u64, Pending> m_pending;
    OwnsRoot m_owns;
    Send m_send;
    Failure m_failure;
    PolicyChanged m_policy_changed;
    u64 m_file_policy_generation { 0 };
    u64 m_next_generation { 0 }, m_next_channel { 0 }, m_next_token { 0 };
    bool m_stopped { false };
    AK::ThreadID m_thread { AK::ThreadID::current() };
};
}
