/** Keep native missing-bitmap/theme paint aligned with the separate renderer canvas policy. */
#include "ContentView.h"
#include "native/canvas/UI.h"
#include <LibGfx/Color.h>
#include <UI/Qt/WebContentView.h>

namespace Firefly {
void initialize_content_background(Ladybird::WebContentView& view, u32 rgb)
{
    // Capture the base pointer while the complete object is alive. QObject's
    // destroyed signal runs after base destruction, so cleanup treats it only
    // as an opaque registry key and never dereferences that pointer.
    auto* key = static_cast<WebView::ViewImplementation const*>(&view);
    QObject::connect(&view, &QObject::destroyed, [key] { CanvasUI::remove(key); });
    set_content_background(view, rgb);
    // The upstream widget starts in Auto. Reapply that initial setting after
    // installing our base policy so its missing-bitmap paint is initialized
    // even before the new native document has acquired a hosting page.
    view.set_preferred_color_scheme(Web::CSS::PreferredColorScheme::Auto);
}

void set_content_background(Ladybird::WebContentView& view, u32 rgb)
{
    VERIFY(rgb <= 0xffffff);
    CanvasUI::set_color(view, Gfx::Color((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255));
    view.update();
}
}
