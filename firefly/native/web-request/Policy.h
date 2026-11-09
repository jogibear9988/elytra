/** UI-owner startup snapshot, serialized before a newly spawned helper executes content. */
#pragma once
#include <AK/Types.h>
#include <LibWebView/Export.h>

namespace Firefly::WebRequest {
struct Policy {
    u32 epoch;
    bool before;
    bool headers;
};

WEBVIEW_API Policy policy();

WEBVIEW_API void set_policy(Policy);
}
