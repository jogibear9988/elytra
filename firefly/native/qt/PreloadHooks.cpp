/**
 * Runs the application preload configurator at native page preparation. Keep configuration on
 * the GUI thread and clear the hook before its host is destroyed.
 */

#include "PreloadHooks.h"

namespace Firefly {
static PreloadConfigurator configurator;

void set_preload_configurator(PreloadConfigurator value)
{
    configurator = move(value);
}

void prepare_preload_view(WebView::ViewImplementation& view, WebView::WebContentPage& page)
{
    if (configurator)
        configurator(view, page);
}
}
