/** Files stay in the main process; queued application callbacks carry bounded metadata only. */
#include "Files.h"
#include "native/protocol/Diagnostics.h"
#include "native/protocol/OriginPolicy.h"
#include <AK/JsonObject.h>
#include <LibURL/Parser.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>

namespace Firefly::Protocols {
FileRoutes::FileRoutes(OwnsRoot owns, Send send, Failure failure, PolicyChanged policy_changed)
    : m_owns(move(owns))
    , m_send(move(send))
    , m_failure(move(failure))
    , m_policy_changed(move(policy_changed))
{
}

FileRoutes::~FileRoutes()
{
    stop();
}

ErrorOr<Optional<u64>> FileRoutes::register_route(StringView scheme, bool modern)
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_stopped || (!is_standard_scheme(scheme) && !(modern && scheme == "file"sv)))
        return Error::from_string_literal(
            "File routes require a registered standard custom scheme");
    if (m_routes.contains(scheme))
        return Optional<u64> {};
    if (m_routes.size() >= 32 || m_next_generation == NumericLimits<u64>::max())
        return Error::from_string_literal("File route budget exhausted");
    if (modern && scheme == "file"sv && m_file_policy_generation >= NumericLimits<u64>::max() - 1)
        return Error::from_string_literal("File interception policy generation exhausted");
    auto generation = ++m_next_generation;
    TRY(m_routes.try_set(ByteString(scheme), Route { generation, modern }));
    if (modern && scheme == "file"sv) {
        ++m_file_policy_generation;
        if (m_policy_changed)
            m_policy_changed(m_file_policy_generation, true);
    }

    return Optional<u64> { generation };
}

bool FileRoutes::has_route(StringView scheme) const
{
    return !m_stopped && m_routes.contains(scheme);
}

bool FileRoutes::intercepts_file() const
{
    auto route = m_routes.find("file"sv);
    return !m_stopped && route != m_routes.end() && route->value.modern;
}

bool FileRoutes::unregister_route(StringView scheme)
{
    VERIFY(m_thread == AK::ThreadID::current());
    bool file = scheme == "file"sv && intercepts_file();
    if (!m_routes.remove(scheme))
        return false;
    if (file) {
        ++m_file_policy_generation;
        if (m_policy_changed)
            m_policy_changed(m_file_policy_generation, false);
    }

    Vector<u64> retired;
    for (auto const& channel : m_channels)
        if (channel.value.url.scheme() == scheme || channel.value.targets.contains(scheme))
            retired.append(channel.key);
    for (auto id : retired) {
        failed(id);
        retire(id);
    }

    return true;
}

void FileRoutes::retire(u64 id)
{
    VERIFY(m_thread == AK::ThreadID::current());
    auto found = m_channels.find(id);
    if (found == m_channels.end())
        return;
    auto server = found->value.server;
    m_channels.remove(found);
    // Cancellation can reenter reply(), but the channel is already unreachable.
    server->revoke();
}

void FileRoutes::sweep()
{
    VERIFY(m_thread == AK::ThreadID::current());
    Vector<u64> retired;
    for (auto const& channel : m_channels)
        if (!channel.value.server->is_open() || !m_owns(channel.value.root))
            retired.append(channel.key);
    for (auto id : retired)
        retire(id);
}

void FileRoutes::stop()
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_stopped)
        return;
    m_stopped = true;
    revoke_weak_ptrs();
    // Make every lookup terminal before reader cancellation/application callbacks.
    auto pending = move(m_pending);
    m_pending = {};
    auto channels = move(m_channels);
    m_channels = {};
    for (auto& channel : channels)
        channel.value.server->revoke();
    for (auto& entry : pending)
        if (entry.value.pull)
            (void)entry.value.pull(Error::from_errno(ECANCELED));
    m_routes.clear();
    m_owns = {};
    m_send = {};
    m_failure = {};
    m_policy_changed = {};
}

ErrorOr<void> FileRoutes::navigation(
    WebView::WebContentPage& page, Web::HTML::NavigationPopulationRequest const& request)
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_stopped || !m_owns(request.navigable_id))
        return {};
    sweep();
    Vector<u64> retired;
    for (auto const& channel : m_channels)
        if (channel.value.root == request.navigable_id && !channel.value.document)
            retired.append(channel.key);
    for (auto id : retired)
        retire(id);
    if (!has_route(request.history_entry.url.scheme())) {
        if (is_standard_scheme(request.history_entry.url.scheme()) && m_failure)
            m_failure(request.navigable_id, request.navigation_id, request.history_entry.url);
        return {};
    }

    auto granted = grant(page, request, {});
    if (granted.is_error() && m_failure)
        m_failure(request.navigable_id, request.navigation_id, request.history_entry.url);
    return granted;
}

ErrorOr<void> FileRoutes::document(WebView::WebContentPage& page,
    Web::HTML::NavigationPopulationRequest const& request,
    Web::HTML::NavigationPopulationResult const& result)
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_stopped || !m_owns(request.navigable_id))
        return {};
    // Replacement revokes old document routes even when the new URL is built-in.
    Vector<u64> retired;
    for (auto const& channel : m_channels)
        if (channel.value.root == request.navigable_id && channel.value.document)
            retired.append(channel.key);
    for (auto id : retired)
        retire(id);
    auto const* params
        = result.navigation_params.get_pointer<Web::HTML::NavigationParamsDescriptor>();
    auto file_route = m_routes.find("file"sv);
    bool intercepted_file = file_route != m_routes.end() && file_route->value.modern;
    if ((!has_route(request.history_entry.url.scheme()) && !intercepted_file)
        || !result.document_id.has_value() || !params
        || params->response.network_error_message.has_value())
        return {};
    auto granted = grant(page, request, result.document_id);
    if (granted.is_error() && m_failure)
        m_failure(request.navigable_id, request.navigation_id, request.history_entry.url);
    return granted;
}

ErrorOr<void> FileRoutes::refresh_document(WebView::WebContentPage& page)
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_stopped || !intercepts_file())
        return {};
    auto& root = page.traversable();
    auto& document = root.active_document();
    if (!m_owns(root.id()) || document.host() != &page)
        return {};
    sweep();
    Vector<u64> retired;
    for (auto const& channel : m_channels) {
        if (!channel.value.document || channel.value.root != root.id())
            continue;
        if (channel.value.document_id == document.id())
            return {}; // The existing document channel already accepts current target routes.
        retired.append(channel.key);
    }

    for (auto id : retired)
        retire(id);
    return grant(page, root.id(),
        root.active_document_load().navigation_id.value_or(Utf16String {}),
        root.active_document_url(), document.id());
}

ErrorOr<void> FileRoutes::grant(WebView::WebContentPage& page,
    Web::HTML::NavigationPopulationRequest const& request, Optional<Root> document)
{
    return grant(
        page, request.navigable_id, request.navigation_id, request.history_entry.url, document);
}

ErrorOr<void> FileRoutes::grant(WebView::WebContentPage& page, Root root,
    Utf16String const& navigation, URL::URL const& url, Optional<Root> document)
{
    sweep();
    if (m_channels.size() >= 64 || m_next_channel == NumericLimits<u64>::max())
        return Error::from_string_literal("Native file channel budget exhausted");
    auto route = m_routes.find(url.scheme());
    auto file_route = m_routes.find("file"sv);
    if (route == m_routes.end()
        && !(document.has_value() && file_route != m_routes.end() && file_route->value.modern))
        return Error::from_string_literal("Native file route has ended");
    auto epoch = ++m_next_channel;
    auto pair = TRY(IPC::Transport::create_paired());
    auto weak = make_weak_ptr();
    auto server = TRY(FileServer::create_responses(
        move(pair.local), epoch,
        [weak, epoch](auto request, auto complete) {
            if (auto self = weak.strong_ref())
                self->request(epoch, move(request), move(complete));
            else
                (void)complete(Error::from_errno(ECANCELED));
        },
        [weak, epoch](u32 request) {
            if (auto self = weak.strong_ref())
                self->cancel(epoch, request);
        },
        [weak, epoch](u32) {
            if (auto self = weak.strong_ref())
                self->failed(epoch);
        }));
    TRY(m_channels.try_set(epoch,
        Channel { root, navigation, url, document.has_value(), document, move(server), {} }));
    if (document.has_value())
        page.client().async_firefly_connect_document(move(pair.remote_handle), page.id().value(),
            root.namespace_id, root.local_id, document->namespace_id, document->local_id,
            navigation, url.serialize(), epoch);
    else
        page.client().async_firefly_connect_navigation(move(pair.remote_handle), page.id().value(),
            root.namespace_id, root.local_id, navigation, url.serialize(), epoch);
    return {};
}

void FileRoutes::request(
    u64 channel, FileServer::AuthorizationRequest request, FileServer::ResponseCompletion complete)
{
    VERIFY(m_thread == AK::ThreadID::current());
    auto found = m_channels.find(channel);
    auto url = URL::Parser::basic_parse(request.url);
    auto target = url.has_value() ? m_routes.find(url->scheme()) : m_routes.end();
    bool intercepted_file = url.has_value() && url->scheme() == "file"sv && target != m_routes.end()
        && target->value.modern;
    if (m_stopped || found == m_channels.end() || !m_owns(found->value.root) || !url.has_value()
        || (!url->origin().is_same_origin(found->value.url.origin()) && !intercepted_file
            && (!found->value.document || !is_cors_scheme(url->scheme())))
        || (!found->value.document && !url->equals(found->value.url, URL::ExcludeFragment::Yes))
        || m_pending.size() >= process_file_request_limit
        || m_next_token == NumericLimits<u64>::max()) {
        diagnostic(DiagnosticStage::RouteScope, m_pending.size(), EACCES);
        (void)complete(Error::from_errno(EACCES));
        return;
    }

    // Source identity belongs to the document; authorization belongs to the
    // current target route, which may have a different scheme/generation.
    auto route = m_routes.find(url->scheme());
    if (route == m_routes.end()
        || found->value.targets.try_set(ByteString(url->scheme())).is_error()) {
        diagnostic(DiagnosticStage::RouteMissing, m_pending.size(), EACCES);
        (void)complete(Error::from_errno(EACCES));
        return;
    }

    auto token = ++m_next_token;
    JsonObject metadata;
    metadata.set("token"sv, String::number(token));
    metadata.set("generation"sv, String::number(route->value.generation));
    metadata.set("scheme"sv, url->scheme());
    metadata.set("url"sv, request.url.view());
    metadata.set("method"sv, request.head ? "HEAD"sv : "GET"sv);
    if (!url->origin().is_same_origin(found->value.url.origin()))
        metadata.set("origin"sv, found->value.url.origin().serialize());
    metadata.set("modern"sv, route->value.modern);
    if (m_pending
            .try_set(token,
                Pending { channel, request.id, move(complete), route->value.generation,
                    route->value.modern, false, false, 0, {} })
            .is_error()) {
        failed(channel);
        retire(channel);
        return;
    }

    m_send(token, "protocol-request", metadata.serialized().to_byte_string());
}

void FileRoutes::cancel(u64 channel, u32 request)
{
    VERIFY(m_thread == AK::ThreadID::current());
    Optional<u64> token;
    for (auto const& pending : m_pending)
        if (pending.value.channel == channel && pending.value.request == request) {
            token = pending.key;
            break;
        }

    if (!token.has_value())
        return;
    finish(*token, true);
}

void FileRoutes::finish(u64 token, bool notify)
{
    VERIFY(m_thread == AK::ThreadID::current());
    auto pending = m_pending.find(token);
    if (pending == m_pending.end())
        return;
    auto value = move(pending->value);
    m_pending.remove(pending);
    // Revoke callback ownership before native completion or application reentry.
    if (value.pull)
        (void)value.pull(Error::from_errno(ECANCELED));
    if (value.complete)
        (void)value.complete(Error::from_errno(ECANCELED));
    if (!notify || !value.delivered || m_stopped || !m_send)
        return;
    JsonObject metadata;
    metadata.set("token"sv, String::number(token));
    metadata.set("generation"sv, String::number(value.generation));
    m_send(token, "protocol-cancel", metadata.serialized().to_byte_string());
}

void FileRoutes::failed(u64 id)
{
    VERIFY(m_thread == AK::ThreadID::current());
    auto channel = m_channels.find(id);
    if (channel != m_channels.end() && !channel->value.document && m_failure)
        m_failure(channel->value.root, channel->value.navigation, channel->value.url);
}

bool FileRoutes::is_pending(u64 token) const
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_stopped)
        return false;
    auto pending = m_pending.find(token);
    if (pending == m_pending.end())
        return false;
    auto channel = m_channels.find(pending->value.channel);
    return channel != m_channels.end() && channel->value.server->is_open()
        && m_owns(channel->value.root);
}

bool FileRoutes::mark_delivered(u64 token)
{
    auto pending = m_pending.find(token);
    if (!is_pending(token) || pending == m_pending.end() || pending->value.delivered)
        return false;
    pending->value.delivered = true;
    return true;
}

bool FileRoutes::reply(u64 token, ErrorOr<AuthorizedFile> file)
{
    VERIFY(m_thread == AK::ThreadID::current());
    auto pending = m_pending.find(token);
    if (pending == m_pending.end() || pending->value.modern)
        return false;
    auto value = move(pending->value);
    m_pending.remove(pending);
    auto channel = m_channels.find(value.channel);
    if (channel == m_channels.end() || !m_owns(channel->value.root)) {
        (void)value.complete(Error::from_errno(ECANCELED));
        return false;
    }

    if (file.is_error())
        return value.complete(file.release_error());
    return value.complete(FileServer::AuthorizedResponse { file.release_value() });
}

bool FileRoutes::reply_response(u64 token, ErrorOr<StreamResponseHead> head)
{
    VERIFY(m_thread == AK::ThreadID::current());
    auto pending = m_pending.find(token);
    if (pending == m_pending.end() || !pending->value.modern)
        return false;
    if (pending->value.streaming) {
        if (!head.is_error())
            return false;
        finish(token, true);
        return true;
    }

    auto channel = m_channels.find(pending->value.channel);
    if (channel == m_channels.end() || !m_owns(channel->value.root)) {
        finish(token, true);
        return false;
    }

    auto complete = move(pending->value.complete);
    if (head.is_error()) {
        diagnostic(DiagnosticStage::RouteResponse, m_pending.size(),
            head.error().is_errno() ? head.error().code() : 0);
        m_pending.remove(pending);
        return complete(head.release_error());
    }

    auto metadata = head.release_value();
    metadata.request = pending->value.request;
    pending->value.streaming = true;
    auto weak = make_weak_ptr();
    AuthorizedStream stream { move(metadata),
        [weak, token](size_t maximum, StreamCompletion complete) {
            if (auto owner = weak.strong_ref())
                owner->pull(token, maximum, move(complete));
            else
                (void)complete(Error::from_errno(ECANCELED));
        },
        [weak, token] {
            if (auto owner = weak.strong_ref())
                owner->finish(token, true);
        } };
    bool accepted = complete(FileServer::AuthorizedResponse { move(stream) });
    if (!accepted)
        finish(token, true);
    return accepted;
}

void FileRoutes::pull(u64 token, size_t maximum, StreamCompletion complete)
{
    VERIFY(m_thread == AK::ThreadID::current());
    auto pending = m_pending.find(token);
    if (m_stopped || pending == m_pending.end() || !pending->value.streaming || pending->value.pull
        || !maximum || maximum > file_chunk_limit) {
        (void)complete(Error::from_errno(ECANCELED));
        return;
    }

    pending->value.credit = maximum;
    pending->value.pull = move(complete);
    JsonObject metadata;
    metadata.set("token"sv, String::number(token));
    metadata.set("generation"sv, String::number(pending->value.generation));
    metadata.set("credit"sv, maximum);
    m_send(token, "protocol-pull", metadata.serialized().to_byte_string());
}

bool FileRoutes::reply_chunk(u64 token, ReadonlyBytes bytes, bool done)
{
    VERIFY(m_thread == AK::ThreadID::current());
    auto pending = m_pending.find(token);
    if (!is_pending(token)) {
        finish(token, true);
        return false;
    }

    if (pending == m_pending.end() || !pending->value.streaming || !pending->value.pull)
        return false;
    if (bytes.size() > pending->value.credit || bytes.size() > file_chunk_limit
        || (bytes.is_empty() && !done)) {
        finish(token, true);
        return false;
    }

    auto copied = ByteBuffer::copy(bytes);
    auto complete = move(pending->value.pull);
    pending->value.credit = 0;
    if (done || copied.is_error())
        m_pending.remove(pending);
    if (copied.is_error())
        return complete(copied.release_error());
    bool accepted = complete(StreamChunk { copied.release_value(), done });
    if (!accepted)
        finish(token, true);
    return accepted;
}
}
