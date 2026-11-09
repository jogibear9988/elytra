/** Revalidate the real document after asynchronous application policy and before replying. */
#include "Adapter.h"
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalNavigable.h>
#include <LibWebView/WebContentPage.h>

namespace Firefly::Permissions {
void dispatch_request(WebView::WebContentPage& page, u64 id, DocumentId document, Name name)
{
    // Match upstream's prompt continuation lifetime: retain the native page
    // until completion, then revalidate its live document before replying.
    NonnullRefPtr owner { page };
    request(page, document, name, [owner = move(owner), id, document, name](auto state) {
        if (!owner->is_open())
            return;
        bool current = false;
        owner->for_each_navigable_with_a_hosted_active_document([&](auto const& navigable) {
            current = navigable.active_document().id() == document;
            return current ? IterationDecision::Break : IterationDecision::Continue;
        });
        if (!current) {
            // The helper Page still owns the original request callback/settings.
            // Deny its monotonic request ID to release those roots; never leave
            // it pending merely because its document has been replaced.
            owner->async_permission_request_completed(id, Web::Permissions::EffectiveState::Denied);
            return;
        }

        if (state == Web::Permissions::EffectiveState::Granted
            && (name == Name::Microphone || !may_use(*owner, document, name)))
            state = Web::Permissions::EffectiveState::Denied;
        owner->async_permission_request_completed(id, state);
    });
}
}
