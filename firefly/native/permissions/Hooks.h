/** Native-only embedder policy seams. No renderer/global registration API. */
#pragma once
#include <AK/Optional.h>
#include <LibWebCommon/HTML/CrossProcessId.h>
#include <LibWebCommon/Permissions/Permissions.h>
#include <LibWebView/Export.h>
#include <functional>

namespace WebView {
class WebContentPage;
}

namespace Firefly::Permissions {
struct Policy {
    u64 epoch {};
    bool check_enabled {};
    bool request_enabled {};
};

using DocumentId = Web::HTML::CrossProcessId;
using Name = Web::Permissions::PermissionName;
using Decision = std::function<void(Web::Permissions::EffectiveState)>;

struct Hooks {
    std::function<Policy()> policy;
    std::function<bool(WebView::WebContentPage&, DocumentId, u64, Name)> check;
    std::function<void(WebView::WebContentPage&, DocumentId, Name, Decision)> request;
    std::function<bool(WebView::WebContentPage&, DocumentId, Name)> may_use;
};

WEBVIEW_API void install_hooks(Hooks);

WEBVIEW_API Policy policy();

WEBVIEW_API bool active();

WEBVIEW_API bool check(WebView::WebContentPage&, DocumentId, u64 epoch, Name);

WEBVIEW_API void request(WebView::WebContentPage&, DocumentId, Name, Decision);

WEBVIEW_API bool may_use(WebView::WebContentPage&, DocumentId, Name);
}
