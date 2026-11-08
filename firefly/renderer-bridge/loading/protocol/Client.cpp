/** Map actual FetchController credit/cancellation to bounded native IPC, without file IO. */
#include "Client.h"
#include "Dispatch.h"
#include "native/protocol/FilePolicy.h"
#include "native/protocol/OriginPolicy.h"
#include "native/protocol/files/FilePacket.h"
#include <AK/NumericLimits.h>
#include <LibCore/EventLoop.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Scripting/Environments.h>

namespace Firefly::ProtocolFetch {
Client::Client(u64 epoch, Web::DOM::Document& document)
    : m_document(document)
    , m_origin(document.origin())
    , m_epoch(epoch)
{
}

ErrorOr<NonnullRefPtr<Client>> Client::create(
    NonnullOwnPtr<IPC::Transport> transport, u64 epoch, Web::DOM::Document& document)
{
    auto owner = adopt_ref(*new Client(epoch, document));
    if (!epoch || !owner->scope_is_active())
        return Error::from_string_literal("Protocol channel requires an admitted active document");
    TRY(owner->initialize(move(transport)));
    return owner;
}

ErrorOr<NonnullRefPtr<Client>> Client::create_navigation(NonnullOwnPtr<IPC::Transport> transport,
    u64 epoch, Web::HTML::LocalNavigable& navigable, URL::URL url, Utf16String id)
{
    if (!epoch)
        return Error::from_string_literal("Invalid navigation channel epoch");
    auto navigation = TRY(Navigation::create(navigable, move(url), move(id)));
    auto owner = adopt_ref(*new Client(epoch, *navigable.active_document()));
    owner->m_navigation = move(navigation);
    TRY(owner->initialize(move(transport)));
    return owner;
}

ErrorOr<void> Client::initialize(NonnullOwnPtr<IPC::Transport> transport)
{
    auto weak = make_weak_ptr();
    m_channel = TRY(Bytes::Channel::create(move(transport),
        {
            .on_data =
                [weak](ReadonlyBytes bytes) {
                    if (auto self = weak.strong_ref())
                        self->receive(bytes);
                },
            .on_writable =
                [weak] {
                    if (auto self = weak.strong_ref())
                        self->schedule();
                },
            .on_closed =
                [weak](auto) {
                    if (auto self = weak.strong_ref())
                        self->revoke();
                },
        }));
    m_timer = Core::Timer::create_repeating(AK::Duration::from_seconds(1), [weak] {
        if (auto self = weak.strong_ref())
            self->sweep();
    });
    m_timer->start();
    return {};
}

Client::~Client()
{
    revoke();
}

bool Client::scope_is_active() const
{
    if (m_navigation)
        return !m_revoked && m_navigation->active();
    if (m_revoked || !m_document || !m_document->is_fully_active() || !m_document->navigable()
        || !m_document->navigable()->is_top_level_traversable()
        || !m_origin.is_same_origin(m_document->origin()))
        return false;
    if (Protocols::file_policy_snapshot().active)
        return true; // The caller also checks exact native document/settings identity per request.
    return !m_origin.is_opaque() && m_origin.is_same_origin(m_document->url().origin())
        && Protocols::is_standard_scheme(m_document->url().scheme());
}

void Client::revoke()
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_revoked)
        return;
    m_revoked = true;
    if (m_timer)
        m_timer->stop();
    if (m_channel)
        m_channel->close();
    auto jobs = move(m_jobs);
    for (auto& entry : jobs)
        if (entry.value.response)
            entry.value.response->stop();
    m_navigation = nullptr;
    m_document = nullptr;
}

ErrorOr<NonnullRefPtr<Response>> Client::fetch(
    Web::Fetch::Infrastructure::FetchParams const& params)
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (!scope_is_active() || !m_channel->is_open()) {
        revoke();
        return Error::from_string_literal("Protocol document channel is revoked");
    }

    auto const& request = *params.request();
    auto const* address = request.client().has_value()
        ? request.client()->get_pointer<Web::HTML::EnvironmentSettingsObjectAddress>()
        : nullptr;
    // Origin equality cannot distinguish two live same-origin documents. Compare
    // the native local settings identity too; remote/worker clients are refused.
    bool admitted = m_navigation ? m_navigation->admit(params)
                                 : allows_request(params) && address
            && *address == m_document->relevant_settings_object().address()
            && request.origin().has<URL::Origin>()
            && request.origin().get<URL::Origin>().is_same_origin(m_origin);
    if (!admitted || m_jobs.size() >= Protocols::document_file_request_limit
        || m_next == NumericLimits<u32>::max())
        return Error::from_string_literal("Protocol request is outside the admitted scope");
    auto url = request.current_url().serialize();
    if (url.bytes().size() > Protocols::file_request_url_limit)
        return Error::from_string_literal("Protocol request URL exceeds the transport limit");
    auto id = ++m_next;
    auto weak = make_weak_ptr();
    auto response = TRY(Response::create(params, m_epoch, id,
        {
            .credit =
                [weak, id](u64 count) {
                    if (auto self = weak.strong_ref())
                        self->credit(id, count);
                },
            .cancel =
                [weak, id] {
                    if (auto self = weak.strong_ref())
                        self->cancel(id);
                },
        },
        m_navigation ? Protocols::navigation_body_limit : Protocols::file_size_limit));
    TRY(m_jobs.try_set(id,
        Job { response, url.to_byte_string(), MonotonicTime::now() + AK::Duration::from_seconds(30),
            0, request.method() == "HEAD"sv }));
    schedule();
    return response;
}

void Client::credit(u32 id, u64 count)
{
    if (m_revoked)
        return;
    auto found = m_jobs.find(id);
    if (found == m_jobs.end() || found->value.cancelled)
        return;
    if (!count || count > Protocols::file_chunk_limit - found->value.credit) {
        revoke();
        return;
    }

    found->value.credit += count;
    schedule();
}

void Client::cancel(u32 id)
{
    if (m_revoked)
        return;
    auto found = m_jobs.find(id);
    if (found == m_jobs.end())
        return;
    if (!found->value.sent) {
        m_jobs.remove(id);
        return;
    }
    // Keep this bounded retired job until a terminal frame consumes any already
    // in-flight data. Release its VM owner immediately, without granting credit.
    found->value.cancelled = true;
    found->value.response = nullptr;
    found->value.credit = 0;
    schedule();
}

void Client::receive(ReadonlyBytes bytes)
{
    VERIFY(m_thread == AK::ThreadID::current());
    auto packet = Protocols::decode_file_packet(bytes);
    if (!scope_is_active() || packet.is_error() || packet.value().epoch != m_epoch) {
        revoke();
        return;
    }

    auto id = packet.value().request;
    auto found = m_jobs.find(id);
    if (found == m_jobs.end() || !found->value.sent) {
        revoke();
        return;
    }

    if (found->value.cancelled) {
        if (packet.value().kind == Protocols::FilePacketKind::Cancel
            || packet.value().kind == Protocols::FilePacketKind::Complete)
            m_jobs.remove(id);
        return;
    }

    auto response = found->value.response;
    auto result = response->receive(bytes);
    if (result.is_error()) {
        revoke();
        return;
    }
    // Response callbacks may cancel/revoke and invalidate the original map entry.
    if (!m_revoked && response->finished())
        m_jobs.remove(id);
}

void Client::schedule()
{
    if (m_revoked || m_scheduled || m_jobs.is_empty())
        return;
    m_scheduled = true;
    auto weak = make_weak_ptr();
    Core::deferred_invoke([weak] {
        if (auto self = weak.strong_ref())
            self->pump();
    });
}

void Client::pump()
{
    VERIFY(m_thread == AK::ThreadID::current());
    m_scheduled = false;
    if (!scope_is_active() || !m_channel->is_open()) {
        revoke();
        return;
    }

    if (m_channel->pending_send_count() >= Bytes::max_pending)
        return;
    Optional<u32> first, after;
    for (auto const& entry : m_jobs) {
        auto const& job = entry.value;
        if (!(!job.sent || (job.cancelled && !job.cancel_sent) || job.credit))
            continue;
        if (!first.has_value() || entry.key < *first)
            first = entry.key;
        if (entry.key > m_last_pumped && (!after.has_value() || entry.key < *after))
            after = entry.key;
    }

    auto id = after.has_value() ? after : first;
    if (!id.has_value())
        return;
    m_last_pumped = *id;
    auto& job = m_jobs.find(*id)->value;
    using Kind = Protocols::FileRequestKind;
    auto kind = !job.sent ? (job.head ? Kind::Head : Kind::Get)
        : job.cancelled   ? Kind::Cancel
                          : Kind::Credit;
    auto wire = Protocols::encode_file_request({ kind, m_epoch, *id,
        kind == Kind::Credit ? job.credit : 0, !job.sent ? job.url.bytes() : ReadonlyBytes {} });
    if (wire.is_error() || m_channel->send(wire.value()).is_error()) {
        revoke();
        return;
    }

    job.sent = true;
    job.url = {};
    if (kind == Kind::Credit)
        job.credit = 0;
    if (kind == Kind::Cancel)
        job.cancel_sent = true;
    schedule();
}

void Client::sweep()
{
    if (!scope_is_active() || !m_channel->is_open()) {
        revoke();
        return;
    }

    auto now = MonotonicTime::now();
    for (auto const& entry : m_jobs)
        if (now >= entry.value.deadline) {
            revoke();
            return;
        }
}
}
