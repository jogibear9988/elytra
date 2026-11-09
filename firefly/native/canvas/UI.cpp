/** Native UI-owner registry. QObject destruction removes the opaque key after the view base dies.
 */
#include "UI.h"
#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <AK/ThreadID.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/ViewImplementation.h>
#include <LibWebView/WebContentPage.h>

namespace Firefly::CanvasUI {
namespace {
    struct Registry {
        AK::ThreadID owner { AK::ThreadID::current() };
        HashMap<WebView::ViewImplementation const*, Gfx::Color> colors;
    };

    Registry& registry()
    {
        static NeverDestroyed<Registry> value;
        return *value;
    }
}

void set_color(WebView::ViewImplementation& view, Gfx::Color color)
{
    auto& state = registry();
    VERIFY(state.owner == AK::ThreadID::current());
    VERIFY(color.alpha() == 255);
    VERIFY(state.colors.contains(&view) || state.colors.size() < 256);
    state.colors.set(&view, color);
    if (view.has_traversable()) {
        if (auto page = view.traversable().active_document().host(); page && page->is_open())
            static_cast<WebContentClientPageStub&>(*page).did_change_background_color(color);
    }
}

void remove(WebView::ViewImplementation const* view)
{
    auto& state = registry();
    VERIFY(state.owner == AK::ThreadID::current());
    state.colors.remove(view);
}

Optional<Gfx::Color> color(WebView::ViewImplementation const& view)
{
    auto& state = registry();
    VERIFY(state.owner == AK::ThreadID::current());
    return state.colors.get(&view);
}
}
