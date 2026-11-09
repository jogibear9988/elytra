/** Owned native request completion; custom decisions never enter origin-global stores. */
#pragma once
#include "Hooks.h"

namespace Firefly::Permissions {
WEBVIEW_API void dispatch_request(WebView::WebContentPage&, u64 request, DocumentId, Name);
}
