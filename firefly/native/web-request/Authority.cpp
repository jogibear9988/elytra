/** An application transport cannot manufacture a new navigation or substitute its source page. */
#include "Authority.h"
#include "Diagnostics.h"
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
    auto worker = operation ? operation->worker() : nullptr;
    bool allowed = current.has_value() && *current == label && operation && worker.ptr() == &page;
    if (!allowed) {
        unsigned flags = !current.has_value() ? 1 : *current != label ? 2 : 0;
        flags |= !operation ? 4 : 0;
        flags |= operation && !worker ? 8 : 0;
        flags |= worker && worker.ptr() != &page ? 16 : 0;
        flags |= operation && operation->host().ptr() == &page ? 32 : 0;
        flags |= operation && operation->source_page.ptr() == &page ? 64 : 0;
        rejection_diagnostic("navigation-authority", flags);
    }

    return allowed;
}
}
