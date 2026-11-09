/** Live Firefly views use their own native zoom, independent of browser host settings. */
#pragma once
#include <LibWebView/Export.h>

namespace WebView {
class ViewImplementation;
}

namespace Firefly::Zoom {
WEBVIEW_API void register_view(WebView::ViewImplementation const&);

WEBVIEW_API void remove_view(WebView::ViewImplementation const*);

WEBVIEW_API bool owns_view(WebView::ViewImplementation const&);
}
