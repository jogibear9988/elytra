/** Routes reserved native IPC events before the optional WebDriver observer; never expose them to
 * page JavaScript. */
#include "TransportHooks.h"
#include "native/platform/Diagnostics.h"
#include <AK/Debug.h>
#include <AK/NeverDestroyed.h>
#include <AK/RefPtr.h>
#include <atomic>
#include <cstring>

namespace Firefly {
void diagnose_navigation_phase(char const* phase, int status)
{
    if (!ipc_bytes_diagnostics_enabled() || !phase || status < -1 || status > 1)
        return;
    StringView selected { phase, std::strlen(phase) };
    constexpr StringView allowed[] { "population-entry"sv, "handoff-request"sv,
        "handoff-callback"sv, "handoff-submitted"sv, "adopt-submitted"sv, "adopt-request"sv,
        "adopt-callback"sv, "before-native-document"sv, "after-native-document"sv,
        "before-native-navigation"sv, "after-native-navigation"sv, "before-populate"sv,
        "after-populate"sv };
    bool valid = false;
    for (auto label : allowed)
        valid |= selected == label;
    if (!valid)
        return;
    static std::atomic<unsigned> remaining { 128 };
    auto available = remaining.load(std::memory_order_relaxed);
    while (available
        && !remaining.compare_exchange_weak(available, available - 1, std::memory_order_relaxed)) {
    }

    if (!available)
        return;
    warnln("FIREFLY_NAVIGATION_PHASE phase={} status={}", selected, status);
}

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
    if (handler.get()) {
        if (name == "firefly.ipc.invoke.v1"sv)
            handler.get()(page, navigable, move(params), NativeIpcKind::Invoke);
        else if (name == "firefly.ipc.send.v1"sv)
            handler.get()(page, navigable, move(params), NativeIpcKind::Event);
        else if (name == "firefly.ipc.bind.v1"sv)
            handler.get()(page, navigable, move(params), NativeIpcKind::Bind);
        else if (name == "firefly.ipc.ack.v1"sv)
            handler.get()(page, navigable, move(params), NativeIpcKind::Ack);
    }

    return true;
}
}
