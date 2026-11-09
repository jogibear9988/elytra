/** Canonical navigation authority stays inside the UI library that owns its native methods. */
#pragma once
#include <AK/String.h>
#include <LibWebView/Export.h>

namespace WebView {
class CanonicalNavigable;
class WebContentPage;
}

namespace Firefly::WebRequest {
WEBVIEW_API bool owns_navigation(
    WebView::WebContentPage&, WebView::CanonicalNavigable&, String const& label);
}
