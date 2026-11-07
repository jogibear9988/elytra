# ADR 0126: Connect public file callbacks to trusted default-session grants

Status: local macOS Release actual public main/renderer acceptance. Broader protocol/
session compatibility, remaining Code-OSS services and workbench startup remain open.

## Decision

Add registerFileProtocol, unregisterProtocol and isProtocolHandled to the main
protocol singleton, and expose that same object through session.defaultSession
after readiness. Only registered standard schemes with explicit supportFetchAPI
can install file routes. Origin classification remains the original immutable
native/helper table; fetch authority is granted through route/channel ownership.
Unrequested fetch capability cannot follow from a standard-only registration.

The maintained ESM callback module owns handler generations and pending tokens.
Native code queues URL/method metadata through the existing VM-owner event dispatch;
never invoke application JavaScript from transport delivery. Bind callbacks to a
generation, tolerate asynchronous/out-of-order replies and reject duplicate/stale
responses. Cancelled queued requests are dropped before entering a busy VM; only
actually delivered callbacks need cancellation messages. Handler throws/rejected
promises refuse requests. Request headers and
initiator fields are not fabricated. Only URL and GET/HEAD are currently supplied.

A native FileRoutes owner independently tracks registered schemes, owned canonical
window roots, bounded channels and pending reply tokens. Before normal navigation
population, grant a channel for that exact URL/intent. Before the selected host
populates a document, grant a second endpoint with upstream's actual document ID.
The existing helper validates page/navigable/document/settings identity. Main also
requires initial-navigation URL equality and same-origin resources. No vendor edits,
origin overrides or custom public page transport are introduced.

Only main authorizes file paths. Reply parsing independently bounds exact metadata,
path and byte-preserving header pairs before calling the checked native file service.
Bytes/descriptors stay out of main JavaScript and are streamed with existing quota/
credit/deadline ownership. Native open/header failures notify the current pending
window load as failures; explicit refusals do likewise. Preserve no page globals.

Unregistering revokes matching channels and cancels native/application pending state.
Old tokens/generations cannot affect a replacement handler. Native window/root loss,
document replacement, peer closure, sweep and shutdown release channels. Clear
hooks/timer/service callbacks before main VM teardown. The current implementation
requires navigation/reload after re-registration to bind an existing document anew;
no in-place capability refresh is claimed.

## Evidence and limits

The public gate launches one actual hidden BrowserWindow. Ordinary public JavaScript
registers the scheme and delayed file handler; no test-only authorizer or endpoint
is installed. Real HTML loads external/nested ESM, applied CSS, binary GET/HEAD and
denial, and verifies every one of 4,194,323 bytes. Renderer code confirms secure
app origin, filtered Set-Cookie and absent Node/Firefly globals. Cross-origin and
POST fetch fail without reaching main's handler. Duplicate callbacks are refused.

Explicit denial and an actual missing file reject loadURL. Removing a handler
settles pending navigation and document fetch, stale callbacks fail before and after
re-registration, and navigation through the replacement succeeds. Pure Node/Mite
callback-policy tests separately cover generation mismatch, validation, readiness,
throws/rejections and cancellation. Original native file/response/fetch regression
gates remain required. Local macOS Release passes; fresh hosted execution is needed.
Mac Release requires this gate; Linux runs it under its existing native Xvfb setup.

This implements a file-callback/default-session subset, not generic Response
handlers, CORS, service workers, native cache, file: interception, custom partitions
or workers/frames. Code-OSS still stops at missing crashReporter and requires those
additional contracts. No workbench launch, general Electron parity or Windows desktop
support is accepted here. Mite remains independent of Firefly/LibWeb.
