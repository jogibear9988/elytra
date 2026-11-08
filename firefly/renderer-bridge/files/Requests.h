/** One-shot native chooser request ownership; not an application/preload interface. */
#pragma once
#include <AK/Function.h>
#include <AK/JsonValue.h>
#include <LibWeb/Export.h>
#include <LibWebCommon/HTML/CrossProcessId.h>
#include <LibWebCommon/HTML/SelectedFile.h>

namespace Web {
class Page;
}

namespace Web::HTML {
class HTMLInputElement;
}

namespace Firefly::FileRequests {
struct Ticket {
    u64 serial;
    Web::HTML::CrossProcessId document;
};

// Native sender callbacks enqueue/capture only; they must not reenter Page delivery.
using Sender = Function<void(Web::Page&, Ticket)>;
WEB_API void set_sender(Sender);
// Called only after the engine installs its pending input target. Without a
// sender, keep the upstream browser/SDK picker path unchanged.
WEB_API bool requested(Web::Page&, Web::HTML::HTMLInputElement&, Web::HTML::AllowMultipleFiles);

WEB_API void closed(Web::Page&);

WEB_API bool allows(Web::HTML::HTMLInputElement&, Span<Web::HTML::SelectedFile>);
// A stale/foreign ticket returns false and never consumes a current request.
// A matching ticket with invalid payload cancels only that original selection.
WEB_API bool deliver(Web::Page&, Ticket, JsonValue const&, Vector<Web::HTML::SelectedFile>&);

WEB_API size_t pending_count();
}
