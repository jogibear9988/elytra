/** Preserve upstream fetch policy; expose no provider registration to JavaScript or workers. */
#include "Dispatch.h"
#include "native/protocol/OriginPolicy.h"
#include <LibHTTP/HeaderList.h>
#include <LibWeb/Fetch/Infrastructure/ClientContextSnapshot.h>

namespace Firefly::ProtocolFetch {
struct Registration::Provider : RefCounted<Provider> {
    explicit Provider(Handler value)
        : handler(move(value))
    {
    }

    Handler handler;
    AK::ThreadID thread { AK::ThreadID::current() };
    bool active { true };
};

// A worker/other agent thread never acquires the main agent's native provider.
thread_local NeverDestroyed<RefPtr<Registration::Provider>> Registration::s_current;

Registration::Registration(NonnullRefPtr<Provider> provider)
    : m_provider(move(provider))
{
}

ErrorOr<NonnullOwnPtr<Registration>> Registration::install(Handler handler)
{
    if (!handler || s_current.get())
        return Error::from_string_literal(
            "Protocol provider is empty or already installed on this agent");
    auto provider = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) Provider(move(handler))));
    auto registration = TRY(adopt_nonnull_own_or_enomem(new (nothrow) Registration(provider)));
    s_current.get() = move(provider);
    return registration;
}

Registration::~Registration()
{
    revoke();
}

void Registration::revoke()
{
    if (!m_provider)
        return;
    VERIFY(m_provider->thread == AK::ThreadID::current());
    m_provider->active = false;
    if (s_current.get() == m_provider)
        s_current.get() = nullptr;
    m_provider = nullptr;
}

GC::Ptr<Web::Fetch::Fetching::PendingResponse> dispatch(
    Web::Fetch::Infrastructure::FetchParams const& params)
{
    using Request = Web::Fetch::Infrastructure::Request;
    using Client = Web::Fetch::Infrastructure::ClientContext;
    using Pending = Web::Fetch::Fetching::PendingResponse;
    using Record = Web::Fetch::Infrastructure::Response;
    auto provider = Registration::s_current.get();
    auto request = params.request();
    if (!provider || !Protocols::is_standard_scheme(request->current_url().scheme()))
        return nullptr;
    auto deny = [&] {
        return Pending::create(params,
            Record::network_error(
                "Custom protocol request is outside the admitted provider scope"_string));
    };

    auto const& client = request->client_snapshot();
    auto const* origin = request->origin().get_pointer<URL::Origin>();
    auto target = request->current_url().origin();
    if (!provider->active || params.is_canceled() || !client || !origin || target.is_opaque()
        || client->global_kind != Client::GlobalKind::Window
        || client->is_window_of_a_top_level_navigable != Client::IsWindowOfATopLevelNavigable::Yes
        || !Protocols::is_standard_scheme(client->creation_url.scheme())
        || !origin->is_same_origin(target) || !client->origin.is_same_origin(target)
        || !client->creation_url.origin().is_same_origin(target) || request->is_navigation_request()
        || request->mode() == Request::Mode::WebSocket
        || !request->method().is_one_of("GET"sv, "HEAD"sv)
        || request->header_list()->contains("Range"sv))
        return deny();
    if (request->destination().has_value()) {
        switch (*request->destination()) {
        case Request::Destination::Worker:
        case Request::Destination::SharedWorker:
        case Request::Destination::ServiceWorker:
        case Request::Destination::AudioWorklet:
        case Request::Destination::PaintWorklet:
        case Request::Destination::Document:
        case Request::Destination::Frame:
        case Request::Destination::IFrame:
            return deny();
        default:
            break;
        }
    }
    // Retain provider through callback reentrancy, including native revocation.
    auto result = provider->handler(params);
    if (result.is_error() || result.value()->finished())
        return deny();
    auto response = result.release_value();
    GC::Root pending { response->pending() };
    if (!provider->active)
        response->stop();
    return *pending;
}
}
