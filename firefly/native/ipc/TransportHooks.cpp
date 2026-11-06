/** Routes reserved native IPC events before the optional WebDriver observer; never expose them to
 * page JavaScript. */
#include "TransportHooks.h"
#include <AK/NeverDestroyed.h>

namespace Firefly {
static NeverDestroyed<NativeIpcHandler> handler;

void set_native_ipc_handler(NativeIpcHandler value)
{
    handler.get() = move(value);
}

bool receive_native_ipc(WebView::WebContentPage& page, Web::HTML::CrossProcessId navigable,
    String const& name, JsonValue& params)
{
    if (!name.starts_with_bytes("firefly.ipc."sv))
        return false;
    if (name == "firefly.ipc.invoke.v1"sv && handler.get())
        handler.get()(page, navigable, move(params));
    return true;
}
}
