# ADR 0121: One-shot native navigation response population

Status: local macOS native parent/child navigation population accepted. Document
commit, live WebContent bootstrap and public protocol handlers remain unaccepted.

## Decision

Add a separate native navigation handler slot to the scheme provider, empty by
default. Existing providers still reject navigation. Share the existing bounded
Client transport implementation, but give navigation a distinct admission/lifetime
owner rather than relax the active-document resource policy. Keep the C++ scope
beside the C++ engine/GC/IPC integration; no additional FFI or JavaScript protocol
layer is needed. Ladybird and standalone Mite remain unchanged.

The trusted native caller supplies an already-current top-level local navigable,
nonempty expected navigation ID, registered-standard nonopaque target URL and an
admitted transport/epoch. The scope roots the original source document and native
navigable. Before the first request, require that source to remain active/current.
Admit exactly once, checking all of:

- GET, navigate mode, document destination and manual redirects;
- no request body, Range, local/remote source client or client snapshot;
- a reserved top-level environment with the expected creation/top-level URL;
- exact native traversable ID, replaced settings-client ID and expected URL;
- the same ongoing native navigation ID and a live top-level navigable.

Browser UI navigation has no source fetch client. Page/remote-client navigation,
workers, subframes, other navigables and URL/ID reuse gain no permission. Keep
generic shape checks in the engine adapter and exact native identity/one-shot
checks in Navigation. A nonempty handler does not itself authorize a request.

Navigation lifetime follows the native transaction instead of requiring the
source document to stay active after commit. Replacement IDs or destroyed roots
invalidate it. Once observed, the destination must have the expected URL/origin
and remain the same active document; a completed ID is accepted only with that
destination. These post-commit branches still need real commit acceptance.
Controller cancellation, explicit revocation, channel failure and fixed request
deadlines use the existing actual response/stream teardown.

The pinned UI population path serializes a body without a RequestServer transfer
lease into NavigationParamsDescriptor bytes, fully reading it before the UI's
commit phase. Bound initial navigation responses to 16 MiB at metadata admission,
before creating a body channel/granting credit. Add a maximum length to the shared
response owner with its existing 64-GiB default for ordinary resource responses.
Do not advertise zero-copy navigation transfer. The later initial-document path
may add a native transfer lease instead of this bounded buffering.

## Evidence and limits

check-firefly-protocol-navigation starts a real native child with LibWeb's principal
realm, initial about:blank document and actual top-level navigable. It uses the
same exported source-snapshot deserializer as native UI requests, then calls the
real LocalNavigable::populate_session_history_entry_document. Upstream constructs
the FetchParams/controller/reserved environment; the test does not fabricate those
or change any document URL/origin. Only the parent knows and opens the authorized
Unicode HTML filename.

Verify the actual returned navigation descriptor's byte-exact 256-KiB + 19-byte
body, target app://bundle origin, native identity, reserved environment and parsed
response CSP. Exactly 16 MiB also succeeds and 16 MiB + 1 refuses before body
delivery. No explicit navigation handler, a serialized page source client, a
different expected URL, a second real native navigable with the same URL/ID and a
replayed request all reject without extra file admissions. Native revocation and
replacement after real headers, peer loss and oversize settle the population and
release every job/file. Read-only response state observes headers without taking
the single PendingResponse callback away from the engine.

Require both the native child result and parent authorization/reaping markers.
Require the new gate through existing check-firefly-protocol-fetch, preserving
macOS release/Linux/experimental Windows workflow entry points. Existing document
JavaScript fetch/stream/abort/quota gates, main-side file service checks and Firefly
compilation pass separately locally. Native Windows/Linux still need fresh hosted
execution. No desktop windows open in these tests.

This accepts response population only. The initial source stays about:blank with
its opaque origin; the test does not create or activate an app-origin DOM document.
The production helper still installs no provider/client/service. Next deliver the
trusted expected URL/navigation/navigable/epoch/transport through the native control
connection before population, run the real UI/helper document-commit path, then bind
post-commit resources to the new exact document. Public protocol/session handlers,
cross-origin/worker support and a Code-OSS workbench launch remain open.
