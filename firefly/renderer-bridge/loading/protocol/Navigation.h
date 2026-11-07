/** One native UI navigation intent, separate from an active document's resource authority. */
#pragma once
#include "Dispatch.h"
#include <LibWeb/HTML/LocalNavigable.h>

namespace Firefly::ProtocolFetch {
class Navigation final {
    AK_ALLOC_WITH_KMALLOC;

public:
    static ErrorOr<NonnullOwnPtr<Navigation>> create(
        Web::HTML::LocalNavigable&, URL::URL expected_url, Utf16String navigation_id);

    bool active() const;

    bool admit(Web::Fetch::Infrastructure::FetchParams const&);

private:
    Navigation(Web::HTML::LocalNavigable&, URL::URL, Utf16String);

    GC::Root<Web::HTML::LocalNavigable> m_navigable;
    GC::Root<Web::DOM::Document> m_source;
    mutable GC::Root<Web::DOM::Document> m_destination;
    URL::URL m_url;
    Utf16String m_id;
    bool m_used { false };
};
}
