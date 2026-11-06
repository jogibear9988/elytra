#pragma once
#include <AK/Function.h>
namespace WebView { class ViewImplementation; class WebContentPage; }
namespace Firefly {
using PreloadConfigurator = Function<void(WebView::ViewImplementation&, WebView::WebContentPage&)>;
void set_preload_configurator(PreloadConfigurator);
void prepare_preload_view(WebView::ViewImplementation&, WebView::WebContentPage&);
}
