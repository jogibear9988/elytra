/** Routes reserved native IPC events before the optional WebDriver observer; never expose them to
 * page JavaScript. */
#include "TransportHooks.h"
#include <AK/NeverDestroyed.h>
#include <AK/RefPtr.h>

namespace Firefly {
static NeverDestroyed<NativeIpcHandler> handler;

struct NavigationCallback : RefCounted<NavigationCallback> {
    explicit NavigationCallback(NativeNavigationHandler value)
        : callback(move(value))
    {
    }

    NativeNavigationHandler callback;
};

static NeverDestroyed<RefPtr<NavigationCallback>> navigation_handler;

void set_native_navigation_handler(NativeNavigationHandler value)
{
    navigation_handler.get()
        = value ? adopt_ref(*new NavigationCallback(move(value))) : RefPtr<NavigationCallback> {};
}

void prepare_native_navigation(
    WebView::WebContentPage& page, Web::HTML::NavigationPopulationRequest const& request)
{
    auto owner = navigation_handler.get();
    if (owner)
        owner->callback(page, request);
}

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
