/** Single UI-owner registry; unset/stale callbacks always deny. */
#include "Hooks.h"
#include <AK/NeverDestroyed.h>
#include <LibIPC/FireflyBootstrap.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>

namespace Firefly::Permissions {
static Hooks& hooks()
{
    static NeverDestroyed<Hooks> value;
    return *value;
}

void install_hooks(Hooks value)
{
    hooks() = std::move(value);
    if (!hooks().policy) {
        Bytes::set_permission_check_receiver({});
        return;
    }

    Bytes::set_permission_check_receiver(
        [](IPC::Stub& stub, u64 page_id, u64 ns, u64 local, u64 epoch, String const& permission) {
            auto& endpoint = static_cast<WebContentClientStub&>(stub);
            auto& client = static_cast<WebView::WebContentClient&>(endpoint);
            auto* page = client.page(Web::PageId { page_id });
            auto name = Web::Permissions::permission_name_from_string(permission);
            return page && name.has_value() && check(*page, { ns, local }, epoch, *name);
        });
}

bool active()
{
    return static_cast<bool>(hooks().policy);
}

Policy policy()
{
    auto callback = hooks().policy;
    return callback ? callback() : Policy {};
}

bool check(WebView::WebContentPage& page, DocumentId doc, u64 epoch, Name name)
{
    auto callback = hooks().check;
    return callback && epoch == policy().epoch && callback(page, doc, epoch, name);
}

void request(WebView::WebContentPage& page, DocumentId doc, Name name, Decision decision)
{
    auto callback = hooks().request;
    if (callback)
        callback(page, doc, name, std::move(decision));
    else
        decision(Web::Permissions::EffectiveState::Denied);
}

bool may_use(WebView::WebContentPage& page, DocumentId doc, Name name)
{
    auto callback = hooks().may_use;
    return callback && callback(page, doc, name);
}
}
