/** Preserve upstream fetch policy; expose no provider registration to JavaScript or workers. */
#include "Dispatch.h"
#include "native/protocol/OriginPolicy.h"
#include <LibHTTP/HeaderList.h>
#include <LibWeb/Fetch/Infrastructure/ClientContextSnapshot.h>
#include <LibWeb/Fetch/Infrastructure/HTTP/CORS.h>

namespace Firefly::ProtocolFetch {
struct Registration::Provider : RefCounted<Provider> {
    Provider(Handler value, Handler navigation_value)
        : handler(move(value))
        , navigation(move(navigation_value))
    {
    }

    Handler handler;
    Handler navigation;
    AK::ThreadID thread { AK::ThreadID::current() };
    bool active { true };
};

// A worker/other agent thread never acquires the main agent's native provider.
thread_local NeverDestroyed<RefPtr<Registration::Provider>> Registration::s_current;

Registration::Registration(NonnullRefPtr<Provider> provider)
    : m_provider(move(provider))
{
}

ErrorOr<NonnullOwnPtr<Registration>> Registration::install(Handler handler, Handler navigation)
{
    if ((!handler && !navigation) || s_current.get())
        return Error::from_string_literal(
            "Protocol provider is empty or already installed on this agent");
    auto provider
        = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) Provider(move(handler), move(navigation))));
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

bool allows_request(Web::Fetch::Infrastructure::FetchParams const& params)
{
    using Request = Web::Fetch::Infrastructure::Request;
    using Client = Web::Fetch::Infrastructure::ClientContext;
    auto request = params.request();
    if (!Protocols::is_standard_scheme(request->current_url().scheme()))
        return false;
    auto const& client = request->client_snapshot();
    auto const* origin = request->origin().get_pointer<URL::Origin>();
    auto target = request->current_url().origin();
    if (params.is_canceled() || !client || !origin || target.is_opaque()
        || client->global_kind != Client::GlobalKind::Window
        || client->is_window_of_a_top_level_navigable != Client::IsWindowOfATopLevelNavigable::Yes
        || !Protocols::is_standard_scheme(client->creation_url.scheme())
        || !origin->is_same_origin(client->origin)
        || !client->creation_url.origin().is_same_origin(*origin)
        || request->is_navigation_request() || request->mode() == Request::Mode::WebSocket
        || !request->method().is_one_of("GET"sv, "HEAD"sv)
        || request->header_list()->contains("Range"sv))
        return false;
    if (!origin->is_same_origin(target)
        && (!Protocols::is_cors_scheme(request->current_url().scheme())
            || request->mode() != Request::Mode::CORS
            || request->credentials_mode() == Request::CredentialsMode::Include
            || request->use_cors_preflight()
            || !Web::Fetch::Infrastructure::get_cors_unsafe_header_names(request->header_list())
                .is_empty()))
        return false;
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
            return false;
        default:
            break;
        }
    }

    return true;
}

bool allows_navigation_request(Web::Fetch::Infrastructure::FetchParams const& params)
{
    using Request = Web::Fetch::Infrastructure::Request;
    using Reserved = Web::Fetch::Infrastructure::ReservedClientContext;
    auto const& request = *params.request();
    auto const& reserved = request.reserved_client_snapshot();
    auto const* environment
        = reserved ? reserved->kind.get_pointer<Reserved::Environment>() : nullptr;
    return !params.is_canceled() && Protocols::is_standard_scheme(request.current_url().scheme())
        && !request.current_url().origin().is_opaque() && request.method() == "GET"sv
        && request.mode() == Request::Mode::Navigate
        && request.destination() == Request::Destination::Document
        && request.redirect_mode() == Request::RedirectMode::Manual && !request.client().has_value()
        && !request.client_snapshot() && !request.remote_client_navigation().has_value()
        && request.body().has<Empty>() && !request.header_list()->contains("Range"sv) && environment
        && environment->target_browsing_context == Reserved::TargetBrowsingContext::TopLevel;
}

GC::Ptr<Web::Fetch::Fetching::PendingResponse> dispatch(
    Web::Fetch::Infrastructure::FetchParams const& params)
{
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

    bool navigation = request->is_navigation_request();
    auto& handler = navigation ? provider->navigation : provider->handler;
    if (!provider->active || !handler
        || !(navigation ? allows_navigation_request(params) : allows_request(params)))
        return deny();
    // Retain provider through callback reentrancy, including native revocation.
    auto result = handler(params);
    if (result.is_error() || result.value()->finished())
        return deny();
    auto response = result.release_value();
    GC::Root pending { response->pending() };
    if (!provider->active)
        response->stop();
    return *pending;
}
}
