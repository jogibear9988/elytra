/** Opaque application canvas defaults for the real, final upstream WebContentView widget. */
#pragma once
#include <AK/Types.h>

namespace Ladybird {
class WebContentView;
}

namespace WebView {
class ViewImplementation;
}

namespace Firefly {
// Internal invariant for a live framework-owned widget. Security admission of
// possibly stale pages uses HubFrontend::canonical_view/owns_page instead.
WebView::ViewImplementation& content_engine_view(Ladybird::WebContentView const&);

void initialize_content_zoom(Ladybird::WebContentView&, double factor);

void set_content_zoom(Ladybird::WebContentView&, double factor);

void initialize_content_background(Ladybird::WebContentView&, u32 rgb = 0xffffff);

void set_content_background(Ladybird::WebContentView&, u32 rgb);
}
