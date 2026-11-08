/** Native-only scoped provider for eligible custom-scheme fetches; absent by default. */
#pragma once
#include "Response.h"
#include <AK/NeverDestroyed.h>
#include <AK/NonnullOwnPtr.h>

namespace Firefly::ProtocolFetch {
// Shared native admission policy for the scheme seam and its transport client.
WEB_API bool allows_request(Web::Fetch::Infrastructure::FetchParams const&);

WEB_API bool allows_navigation_request(Web::Fetch::Infrastructure::FetchParams const&);

class WEB_API Registration final {
    AK_ALLOC_WITH_KMALLOC;
    AK_MAKE_NONCOPYABLE(Registration);

    AK_MAKE_NONMOVABLE(Registration);

public:
    using Handler = Function<ErrorOr<NonnullRefPtr<Response>>(
        Web::Fetch::Infrastructure::FetchParams const&)>;
    static ErrorOr<NonnullOwnPtr<Registration>> install(Handler, Handler navigation = {});

    ~Registration();

    void revoke();

private:
    struct Provider;
    static thread_local NeverDestroyed<RefPtr<Provider>> s_current;
    friend GC::Ptr<Web::Fetch::Fetching::PendingResponse> dispatch(
        Web::Fetch::Infrastructure::FetchParams const&);

    explicit Registration(NonnullRefPtr<Provider>);

    RefPtr<Provider> m_provider;
};

// Programmatic CORS file requests otherwise fail before scheme_fetch. This native
// interception consumes only authenticated file-owned requests and never grants
// the upstream filesystem fallback a new destination or origin privilege.
WEB_API GC::Ptr<Web::Fetch::Fetching::PendingResponse> dispatch_file_interception(
    Web::Fetch::Infrastructure::FetchParams const&);

// The owned scheme-fetch seam calls this after upstream cancellation checks.
// Null preserves the upstream path. A provider denial returns a network error.
GC::Ptr<Web::Fetch::Fetching::PendingResponse> dispatch(
    Web::Fetch::Infrastructure::FetchParams const&);
}
