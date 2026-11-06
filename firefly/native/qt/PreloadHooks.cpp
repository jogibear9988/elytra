#include "PreloadHooks.h"

namespace Firefly {
static PreloadConfigurator configurator;
void set_preload_configurator(PreloadConfigurator value) { configurator = move(value); }
void prepare_preload_view(WebView::ViewImplementation& view, WebView::WebContentPage& page)
{
    if (configurator) configurator(view, page);
}
}
