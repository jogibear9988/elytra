/** Per-view opaque canvas defaults; an unregistered upstream view retains its original behavior. */
#pragma once
#include <AK/Optional.h>
#include <LibGfx/Color.h>
#include <LibWebView/Export.h>

namespace WebView {
class ViewImplementation;
}

namespace Firefly::CanvasUI {
WEBVIEW_API void set_color(WebView::ViewImplementation&, Gfx::Color);

WEBVIEW_API void remove(WebView::ViewImplementation const*);

WEBVIEW_API Optional<Gfx::Color> color(WebView::ViewImplementation const&);
}
