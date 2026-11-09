/** UI-thread view membership only; the factor remains in upstream ViewImplementation. */
#include "View.h"
#include <AK/HashTable.h>
#include <AK/NeverDestroyed.h>
#include <AK/ThreadID.h>

namespace Firefly::Zoom {
namespace {
    struct Registry {
        AK::ThreadID owner { AK::ThreadID::current() };
        HashTable<WebView::ViewImplementation const*> views;
    };

    Registry& registry()
    {
        static NeverDestroyed<Registry> state;
        VERIFY(state->owner == AK::ThreadID::current());
        return *state;
    }
}

void register_view(WebView::ViewImplementation const& view)
{
    auto& views = registry().views;
    VERIFY(!views.contains(&view) && views.size() < 256);
    views.set(&view);
}

void remove_view(WebView::ViewImplementation const* view)
{
    registry().views.remove(view);
}

bool owns_view(WebView::ViewImplementation const& view)
{
    return registry().views.contains(&view);
}
}
