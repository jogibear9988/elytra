/** Exercise the policy with real Fetch requests, including disabled helper behavior. */
#include "FileModulePolicyTest.h"
#include "loading/FileModules.h"
#include <AK/ScopeGuard.h>
#include <LibURL/Parser.h>
#include <LibWeb/Fetch/Infrastructure/HTTP/Requests.h>

ErrorOr<void> check_file_module_policy()
{
    using namespace Web::Fetch::Infrastructure;
    auto local = URL::Parser::basic_parse("file:///tmp/firefly/main.mjs"sv).release_value();
    auto network = URL::Parser::basic_parse("https://example.com/"sv).release_value();
    auto request = Request::create();
    request->set_url_list({ local });
    request->set_origin(local.origin());
    request->set_mode(Request::Mode::CORS);
    request->set_destination(Request::Destination::Script);
    auto client_origin = local.origin();
    auto client = ClientContext { .origin = client_origin,
        .site_for_cookies = client_origin,
        .creation_url = local,
        .top_level_creation_url = {},
        .top_level_origin = {},
        .global_kind = ClientContext::GlobalKind::Window,
        .navigable_id = {},
        .referrer_source = {},
        .content_blocker_source_url = local,
        .keepalive_quota_accountant = KeepaliveQuotaAccountant::create() };
    auto snapshot
        = [&] { request->set_client_snapshot(adopt_ref(*new ClientContextSnapshot(client))); };
    snapshot();
    if (Firefly::FileModules::allows(*request))
        return Error::from_string_literal("File modules enabled in an ordinary helper");
    Firefly::FileModules::set_enabled(true);
    ScopeGuard reset { [] { Firefly::FileModules::set_enabled(false); } };
    if (!Firefly::FileModules::allows(*request))
        return Error::from_string_literal("Local window module was refused");
    for (auto destination : { Optional<Request::Destination> {},
             Optional<Request::Destination> { Request::Destination::JSON },
             Optional<Request::Destination> { Request::Destination::Worker },
             Optional<Request::Destination> { Request::Destination::Style } }) {
        request->set_destination(destination);
        if (Firefly::FileModules::allows(*request))
            return Error::from_string_literal(
                "File policy admitted fetch or a non-script destination");
    }

    request->set_destination(Request::Destination::Script);
    request->set_mode(Request::Mode::NoCORS);
    if (Firefly::FileModules::allows(*request))
        return Error::from_string_literal("File policy rewrote classic/no-CORS behavior");
    request->set_mode(Request::Mode::CORS);
    request->set_url_list({ network });
    if (Firefly::FileModules::allows(*request))
        return Error::from_string_literal("File policy admitted a network URL");
    request->set_url_list({ local });
    request->set_origin(network.origin());
    if (Firefly::FileModules::allows(*request))
        return Error::from_string_literal("Network origin gained local scripts");
    request->set_origin(local.origin());
    request->set_client_snapshot(nullptr);
    if (Firefly::FileModules::allows(*request))
        return Error::from_string_literal("Missing client snapshot accepted");
    client.global_kind = ClientContext::GlobalKind::Worker;
    snapshot();
    if (Firefly::FileModules::allows(*request))
        return Error::from_string_literal("Worker client gained file module policy");
    client.global_kind = ClientContext::GlobalKind::Window;
    client.creation_url = network;
    snapshot();
    if (Firefly::FileModules::allows(*request))
        return Error::from_string_literal("Network-created client gained local modules");
    client.creation_url = local;
    client.origin = URL::Origin::create_opaque();
    snapshot();
    if (Firefly::FileModules::allows(*request))
        return Error::from_string_literal("Opaque/sandbox client gained local modules");
    outln("PASS file module policy defaults off; local window scripts only; fetch, network, "
          "workers and opaque clients denied");
    return {};
}
