/** Native host-thread IPC dispatch seam. The window bridge installs and clears the handler during
 * its lifetime. */
#pragma once
#include <AK/Function.h>
#include <AK/JsonValue.h>
#include <AK/String.h>
#include <LibWebCommon/HTML/CrossProcessId.h>
#include <LibWebCommon/HTML/NavigationPopulationRequest.h>
#include <LibWebView/Export.h>

namespace WebView {
class WebContentPage;
}

namespace Firefly {
using NativeIpcHandler
    = Function<void(WebView::WebContentPage&, Web::HTML::CrossProcessId, JsonValue)>;
WEBVIEW_API void set_native_ipc_handler(NativeIpcHandler);

using NativeNavigationHandler
    = Function<void(WebView::WebContentPage&, Web::HTML::NavigationPopulationRequest const&)>;
WEBVIEW_API void set_native_navigation_handler(NativeNavigationHandler);

WEBVIEW_API void prepare_native_navigation(
    WebView::WebContentPage&, Web::HTML::NavigationPopulationRequest const&);

using NativeDocumentHandler = Function<void(WebView::WebContentPage&,
    Web::HTML::NavigationPopulationRequest const&, Web::HTML::NavigationPopulationResult const&)>;
WEBVIEW_API void set_native_document_handler(NativeDocumentHandler);

WEBVIEW_API void prepare_native_document(WebView::WebContentPage&,
    Web::HTML::NavigationPopulationRequest const&, Web::HTML::NavigationPopulationResult const&);

WEBVIEW_API bool receive_native_ipc(
    WebView::WebContentPage&, Web::HTML::CrossProcessId, String const&, JsonValue&);
}
