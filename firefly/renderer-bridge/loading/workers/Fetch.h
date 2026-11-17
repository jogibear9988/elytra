/** Worker-local VM state and native owner-thread brokers for dedicated-worker Fetch. */
#pragma once
#include "loading/web-request/Gate.h"
#include <AK/Function.h>
#include <LibURL/URL.h>
#include <LibWeb/Export.h>
#include <functional>

namespace Firefly::WorkersFetch {
using Params = Web::Fetch::Infrastructure::FetchParams;
using Response = Web::Fetch::Infrastructure::Response;
using Pending = Web::Fetch::Fetching::PendingResponse;

struct Endpoint {
    u64 epoch;
    u32 request;
    std::function<void(u64)> credit;
    std::function<void()> cancel;
};

struct Remote {
    std::function<bool(ReadonlyBytes)> packet;
    std::function<void()> closed;
    std::function<bool()> valid;
};

using Open = Function<ErrorOr<Endpoint>(Web::DOM::Document&, URL::URL const&, bool, Remote)>;
WEB_API void install_client_provider(Open);

WEB_API void enter();

WEB_API void leave();

WEB_API void retire();

WEB_API GC::Ref<Pending> before(JS::Realm&, Params const&, GC::Ref<WebRequestGate::Before>);

WEB_API bool headers(Params const&, NonnullRefPtr<Response>, GC::Ref<WebRequestGate::Head>);

WEB_API void cancel(Params const&);

WEB_API void finish(Params const&);

WEB_API GC::Ref<Pending> dispatch(Params const&);
}
