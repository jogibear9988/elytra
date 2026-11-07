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

struct DocumentCallback : RefCounted<DocumentCallback> {
    explicit DocumentCallback(NativeDocumentHandler value)
        : callback(move(value))
    {
    }

    NativeDocumentHandler callback;
};

static NeverDestroyed<RefPtr<DocumentCallback>> document_handler;

void set_native_document_handler(NativeDocumentHandler value)
{
    document_handler.get()
        = value ? adopt_ref(*new DocumentCallback(move(value))) : RefPtr<DocumentCallback> {};
}

void prepare_native_document(WebView::WebContentPage& page,
    Web::HTML::NavigationPopulationRequest const& request,
    Web::HTML::NavigationPopulationResult const& result)
{
    auto owner = document_handler.get();
    if (owner)
        owner->callback(page, request, result);
}

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
