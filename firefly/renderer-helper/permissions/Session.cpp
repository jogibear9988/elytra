/** No application metadata is trusted: query only the native document currently executing. */
#include "Session.h"
#include "renderer-bridge/permissions/Hooks.h"
#include <AK/WeakPtr.h>
#include <LibIPC/FireflyBootstrap.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/Page/Page.h>
#include <WebContent/ConnectionFromClient.h>
#include <WebContent/PageClient.h>
#include <WebContent/PageHost.h>

namespace Firefly::HelperPermissions {
namespace {
    WeakPtr<WebContent::ConnectionFromClient> parent_connection;

    bool check(Web::DOM::Document& document, u64 epoch, StringView name)
    {
        if (!document.is_fully_active() || !document.navigable())
            return false;
        auto* page = as_if<WebContent::PageClient>(document.page().client());
        if (!page)
            return false;
        auto connection = parent_connection.strong_ref();
        if (!connection || !connection->is_open())
            return false;
        auto hosted = connection->page_host().page(page->id());
        if (!hosted.has_value() || &*hosted != page)
            return false;
        auto id = document.id();
        auto reply
            = connection
                  ->send_sync_but_allow_failure<Messages::WebContentClient::FireflyPermissionCheck>(
                      page->id().value(), id.namespace_id, id.local_id, epoch,
                      MUST(String::from_utf8(name)));
        return reply && reply->granted();
    }
}

ErrorOr<Protocols::OriginArguments> prepare(ReadonlySpan<StringView> arguments)
{
    constexpr auto prefix = "--firefly-permission-policy="sv;
    Protocols::OriginArguments result;
    Optional<RendererPermissions::Policy> selected;
    bool positional = false;
    for (auto argument : arguments) {
        if (!positional && argument.starts_with(prefix)) {
            auto parts = argument.substring_view(prefix.length()).split_view(':');
            if (selected.has_value() || argument.length() > prefix.length() + 16
                || parts.size() != 3)
                return Error::from_string_literal("Invalid/duplicate native permission policy");
            auto epoch = parts[0].to_number<u64>();
            auto canonical = epoch.has_value() ? ByteString::number(*epoch) : ByteString {};
            if (!epoch.has_value() || !*epoch || *epoch > 2147483647 || canonical.view() != parts[0]
                || !parts[1].is_one_of("0"sv, "1"sv) || !parts[2].is_one_of("0"sv, "1"sv))
                return Error::from_string_literal("Invalid native permission policy values");
            selected = RendererPermissions::Policy { *epoch, parts[1] == "1"sv, parts[2] == "1"sv };
            continue;
        }

        if (!positional && argument == "--firefly-permission-policy"sv)
            return Error::from_string_literal("Native permission policy requires a value");
        if (argument == "--"sv)
            positional = true;
        TRY(result.owned.try_append(ByteString(argument)));
    }

    for (auto& value : result.owned) {
        TRY(result.strings.try_append(value.view()));
        TRY(result.argv.try_append(const_cast<char*>(value.characters())));
    }

    TRY(result.argv.try_append(nullptr));
    if (selected.has_value()
        && !RendererPermissions::apply_policy(
            selected->epoch, selected->check_enabled, selected->request_enabled))
        return Error::from_string_literal("Native permission policy regressed");
    RendererPermissions::enable();
    return result;
}

void initialize()
{
    RendererPermissions::set_query(check);
    Bytes::set_permission_policy_receiver([](IPC::Stub& stub, u64 epoch, bool check, bool request) {
        // Only the authenticated main ConnectionFromClient owns this NotRouted stub.
        auto& connection = static_cast<WebContent::ConnectionFromClient&>(
            static_cast<WebContentServerStub&>(stub));
        if (RendererPermissions::apply_policy(epoch, check, request))
            parent_connection = connection.make_weak_ptr<WebContent::ConnectionFromClient>();
    });
}

void shutdown()
{
    RendererPermissions::set_query(nullptr);
    parent_connection = nullptr;
    Bytes::set_permission_policy_receiver({});
    // Keep default-deny enabled: teardown must not reveal cached upstream grants.
}
}
