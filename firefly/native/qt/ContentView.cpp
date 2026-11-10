/** Keep native missing-bitmap/theme paint aligned with the separate renderer canvas policy. */
#include "ContentView.h"
#include "hub/Frontend.h"
#include "native/canvas/UI.h"
#include "native/zoom/View.h"
#include <Hub/View.h>
#include <LibGfx/Color.h>
#include <UI/Qt/WebContentView.h>

namespace Firefly {
WebView::ViewImplementation& content_engine_view(Ladybird::WebContentView const& view)
{
    auto canonical = HubFrontend::canonical_view(view);
    VERIFY(canonical.has_value());
    return *canonical;
}

static void synchronize_frontend(Ladybird::WebContentView& view)
{
    // This is the genuine Hub ViewState, not duplicated application metadata.
    // Initial hidden widgets need it before the queued Hub state event arrives.
    auto& canonical = content_engine_view(view);
    auto* hub_view = dynamic_cast<Hub::View*>(&canonical);
    VERIFY(hub_view);
    view.set_state(hub_view->current_state());
}

void initialize_content_zoom(Ladybird::WebContentView& view, double factor)
{
    auto* key = &content_engine_view(view);
    Zoom::register_view(*key);
    // Qt and Hub ownership are separate. Destruction removes the native registry
    // key before/independently of the queued Hub view teardown, without reading it.
    QObject::connect(&view, &QObject::destroyed, [key] { Zoom::remove_view(key); });
    set_content_zoom(view, factor);
}

void set_content_zoom(Ladybird::WebContentView& view, double factor)
{
    content_engine_view(view).set_zoom(factor);
    synchronize_frontend(view);
    view.did_update_zoom();
}

void initialize_content_background(Ladybird::WebContentView& view, u32 rgb)
{
    // Capture actual Hub identity, not a pointer cast from the presentation widget.
    // Cleanup treats this only as an opaque key and does not dereference it.
    auto* key = &content_engine_view(view);
    QObject::connect(&view, &QObject::destroyed, [key] { CanvasUI::remove(key); });
    set_content_background(view, rgb);
    // The upstream widget starts in Auto. Reapply that initial setting after
    // installing our base policy so its missing-bitmap paint is initialized
    // even before the new native document has acquired a hosting page.
    key->set_preferred_color_scheme(Web::CSS::PreferredColorScheme::Auto);
    synchronize_frontend(view);
}

void set_content_background(Ladybird::WebContentView& view, u32 rgb)
{
    VERIFY(rgb <= 0xffffff);
    CanvasUI::set_color(
        content_engine_view(view), Gfx::Color((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255));
    synchronize_frontend(view);
    view.update();
}
}
