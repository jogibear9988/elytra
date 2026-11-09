/** Private owner-thread Fetch gates. No page, preload or debugger registration API. */
#pragma once
#include <AK/Function.h>
#include <AK/JsonObject.h>
#include <LibGC/Function.h>
#include <LibWeb/Export.h>
#include <LibWeb/Fetch/Fetching/PendingResponse.h>
#include <LibWeb/Fetch/Infrastructure/FetchParams.h>

namespace Web::DOM {
class Document;
}

namespace Firefly::WebRequestGate {
enum class Stage { Before, Headers };

struct Policy {
    u32 epoch {};
    bool before {};
    bool headers {};
};

using Completion = Function<void(Optional<JsonObject>)>;
using Submit = Function<ErrorOr<u32>(Stage, Web::DOM::Document&, JsonObject, Completion)>;
using Cancel = Function<void(u32)>;
using Finish = Function<void(u32)>;
using Before = GC::Function<GC::Ref<Web::Fetch::Fetching::PendingResponse>()>;
using Head = GC::Function<void(NonnullRefPtr<Web::Fetch::Infrastructure::Response>)>;
WEB_API void install(Submit, Cancel, Finish);

WEB_API void shutdown();

WEB_API void sweep();

WEB_API Policy policy();

WEB_API bool configure(Policy);

WEB_API GC::Ref<Web::Fetch::Fetching::PendingResponse> before(
    JS::Realm&, Web::Fetch::Infrastructure::FetchParams const&, GC::Ref<Before>);

WEB_API bool headers(Web::Fetch::Infrastructure::FetchParams const&,
    NonnullRefPtr<Web::Fetch::Infrastructure::Response>, GC::Ref<Head>);

WEB_API void cancel(Web::Fetch::Infrastructure::FetchParams const&);

WEB_API void finish(Web::Fetch::Infrastructure::FetchParams const&);
}
