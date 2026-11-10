/** Exercise the selected host's real document-ID handoff and bounded main-side file routes. */
#include "DocumentResources.h"
#include <AK/LexicalPath.h>
#include <LibCore/EventLoop.h>
#include <LibWebView/WebContentClient.h>

namespace Firefly {
DocumentResources::DocumentResources(ByteString fixture, ByteString mode)
    : m_directory(LexicalPath(fixture).dirname())
    , m_mode(move(mode))
{
}

DocumentResources::~DocumentResources()
{
    revoke();
}

void DocumentResources::send(WebView::WebContentPage& page,
    Web::HTML::NavigationPopulationRequest const& request, Web::HTML::CrossProcessId document,
    IPC::TransportHandle handle, u64 epoch)
{
    page.client().async_firefly_connect_document(move(handle), page.id().value(),
        request.navigable_id.namespace_id, request.navigable_id.local_id, document.namespace_id,
        document.local_id + (m_mode == "document-id"sv ? 9999 : 0), request.navigation_id,
        request.history_entry.url.serialize(), epoch);
}

ErrorOr<void> DocumentResources::prepare(WebView::WebContentPage& page,
    Web::HTML::NavigationPopulationRequest const& request,
    Web::HTML::NavigationPopulationResult const& result)
{
    if (request.history_entry.url.scheme() != "app"sv || m_mode == "document-missing"sv)
        return {};
    auto const* response
        = result.navigation_params.get_pointer<Web::HTML::NavigationParamsDescriptor>();
    if (!response || response->response.network_error_message.has_value()
        || !result.document_id.has_value()
        || request.history_entry.url.serialize() != "app://bundle/index.html"sv || m_server)
        return Error::from_string_literal("Unexpected native document population handoff");
    auto pair = TRY(IPC::Transport::create_paired());
    auto weak = make_weak_ptr();
    auto create_server = [weak](NonnullOwnPtr<IPC::Transport> transport,
                             u64 epoch) -> ErrorOr<NonnullRefPtr<Protocols::FileServer>> {
        return Protocols::FileServer::create_async(move(transport), epoch,
            [weak, epoch](Protocols::FileServer::AuthorizationRequest request,
                Protocols::FileServer::Completion complete) {
                // Keep authorization genuinely asynchronous on the native owner.
                Core::deferred_invoke(
                    [weak, epoch, request = move(request), complete = move(complete)] {
                        auto self = weak.strong_ref();
                        if (!self) {
                            (void)complete(Error::from_errno(ECANCELED));
                            return;
                        }

                        if (epoch == 3)
                            ++self->m_refresh_requests;
                        (void)complete(self->authorize(request.url));
                    });
            });
    };

    m_server = TRY(create_server(move(pair.local), 2));
    send(page, request, *result.document_id, move(pair.remote_handle), 2);
    if (m_mode == "document-duplicate"sv) {
        auto extra = TRY(IPC::Transport::create_paired());
        m_duplicate = TRY(Bytes::Channel::create(move(extra.local),
            {
                .on_data =
                    [weak](auto) {
                        if (auto self = weak.strong_ref())
                            self->m_failed = true;
                    },
                .on_writable = {},
                .on_closed =
                    [weak](auto reason) {
                        if (auto self = weak.strong_ref()) {
                            self->m_duplicate_closed = reason == Bytes::Channel::CloseReason::Peer;
                            if (!self->m_duplicate_closed)
                                self->m_failed = true;
                        }
                    },
            }));
        // An equal epoch is a replay, not a newer trusted grant. A higher epoch
        // intentionally refreshes this exact committed document (ADR 0175).
        send(page, request, *result.document_id, move(extra.remote_handle), 2);
    } else if (m_mode == "document-refresh"sv) {
        auto refresh = TRY(IPC::Transport::create_paired());
        m_retired_server = m_server;
        m_server = TRY(create_server(move(refresh.local), 3));
        send(page, request, *result.document_id, move(refresh.remote_handle), 3);
    }

    return {};
}

ErrorOr<Protocols::AuthorizedFile> DocumentResources::authorize(StringView url)
{
    ++m_requests;
    StringView filename;
    u32 bit = 0;
    if (url == "app://bundle/main.mjs"sv) {
        filename = "main.mjs"sv;
        bit = 1;
    } else if (url == "app://bundle/math.mjs"sv) {
        filename = "math.mjs"sv;
        bit = 2;
    } else if (url == "app://bundle/theme.css"sv) {
        filename = "theme.css"sv;
        bit = 4;
    } else if (url == "app://bundle/payload.bin"sv) {
        filename = "payload.bin"sv;
        bit = 8;
    } else if (url == "app://bundle/denied.bin"sv) {
        m_seen |= 16;
        return Error::from_string_literal("Route explicitly denied");
    } else
        return Error::from_string_literal("URL outside the native bundle routes");
    m_seen |= bit;
    return Protocols::AuthorizedFile { LexicalPath::join(m_directory, filename).string(),
        { { "Cache-Control", "no-store" }, { "Set-Cookie", "fixture=private" } } };
}

bool DocumentResources::ready(bool away) const
{
    if (m_failed || (m_mode == "document-duplicate"sv && !m_duplicate_closed))
        return false;
    if (m_mode == "document-missing"sv)
        return !m_server && !m_requests;
    if (!m_server || m_server->pending() || m_server->files())
        return false;
    if (m_mode == "document-refresh"sv
        && (!m_retired_server || !m_retired_server->is_revoked() || m_retired_server->pending()
            || m_retired_server->files() || m_refresh_requests < 6))
        return false;
    if (m_mode == "document-id"sv)
        return m_server->is_revoked() && !m_requests;
    return m_seen == 31 && m_requests >= 6 && (away ? m_server->is_revoked() : m_server->is_open());
}

void DocumentResources::revoke()
{
    if (m_server)
        m_server->revoke();
    if (m_retired_server)
        m_retired_server->revoke();
    if (m_duplicate)
        m_duplicate->close();
}
}
