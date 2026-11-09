/** An application transport cannot manufacture a new navigation or substitute its source page. */
#include "Authority.h"
#include <AK/Utf16String.h>
#include <LibWebView/CanonicalNavigable.h>
#include <LibWebView/CanonicalNavigation.h>
#include <LibWebView/WebContentPage.h>

namespace Firefly::WebRequest {
bool owns_navigation(
    WebView::WebContentPage& page, WebView::CanonicalNavigable& navigable, String const& name)
{
    if (!page.is_open() || name.is_empty() || name.byte_count() > 4096
        || !navigable.is_top_level_traversable())
        return false;
    auto label = Utf16String::from_utf8_with_replacement_character(name);
    auto current = navigable.ongoing_navigation_id();
    auto operation = navigable.navigation_with_id(label);
    return current.has_value() && *current == label && operation
        && operation->worker().ptr() == &page;
}
}
