# ADR 0119: Document-bound fetch over native resource IPC

Status: local macOS non-GUI parent/child JavaScript component accepted. Live
WebContent helper routing and public protocol/session handlers remain open.

## Decision

Connect the real LibWeb response adapter to the native file service with a small
C++ client over the existing bounded byte channel. C++ directly reuses the C++
FetchController/GC/IPC boundary without another FFI or JavaScript transport. The
client performs no filesystem IO and links separately from LibWeb; the main-side
FileServer and checked-file owner stay outside the engine dylib. Mite and upstream
Ladybird remain unchanged.

The native caller supplies a transport and nonzero epoch admitted for one exact
active top-level document. Creation roots that document, but installs no scheme
provider or page/preload API. A native provider/router may call fetch on the client.
Share the scheme seam's existing request eligibility policy rather than maintain
two diverging GET/HEAD/origin/worker/navigation/Range policies.

Before sending, compare the request's native local settings address with this
document's actual settings identity. Equal origins alone do not authorize a second
same-origin document. Require the document to stay fully active with the original
nonopaque registered-standard origin and top-level navigable. Check this again on
incoming frames, outgoing tasks and the native one-second sweep. Live helper
admission must still verify the real native document/channel association; a packet
epoch is correlation, not permission.

Bound active/retiring jobs with the shared eight-request budget. Allocate monotonic
nonzero u32 IDs; exhaustion fails explicitly. Send bounded URL-only Get/Head frames,
then translate actual FetchBodyDeliveryGate grants into Credit frames. Round-robin
one request/control frame per deferred task and pause on actual transport window
exhaustion. ACKs release only transport credit; upstream body credit governs file
production. The response owner validates actual epoch/ID/header/body/offset/length
and credit before writing the real FetchByteChannel.

A cancelled request not yet sent is removed without opening a main-side file.
After sending, queue Cancel and release the response's VM ownership immediately,
retaining only a bounded retiring job until the terminal Cancel/Complete consumes
already-in-flight frames. Do not grant more body credit. Unknown/unsent response
IDs, malformed frames, wrong epochs or response-adapter errors revoke the channel.
Callbacks retain only weak clients; response delivery retains a local owner across
reentrant cancellation. Never keep a map-entry reference across response callbacks.

Revocation closes the transport, stops every pending real response/stream, stops
the timer and releases document/VM roots. Destruction does the same. Peer loss and
document inactivity settle all requests. A fixed 30-second per-job lifetime also
bounds a silent server or an unacknowledged cancellation; the sweep revokes the
channel and grants do not refresh deadlines. No fake resolved fetch or JS stream
implementation is introduced.

## Evidence and limits

check-firefly-protocol-fetch-channel starts a real native child with an actual
LibWeb principal realm/document. Only its native parent knows/opens the authorized
Unicode file path. Real global fetch/arrayBuffer reads every byte of a 4 MiB + 19
byte file through scheme dispatch, native request/credit IPC, FileServer response
frames and the actual engine body channel. In parallel, HEAD has advertised length
but no body and an unauthorized URL rejects. Actual response filtering hides
Set-Cookie. Page process/require/firefly/electron capabilities remain absent.

Eight parallel bodies complete while a ninth rejects before reaching the service.
A second real same-origin document rejects despite sharing the agent/provider.
Actual AbortController, early abort with zero file admissions, reader.cancel,
native revocation, dropping the client after headers, peer loss, malformed replies,
foreign epochs and future response IDs settle promises/streams and release jobs.
Dropping the native owner proves weak callbacks introduce no retention cycle.
A separate silent native peer exercises the client's actual 30-second timer without
using FileServer's timer. Native parent admission counts and reaped child status
are required alongside the JavaScript acceptance marker. GC follows wait boundaries.

The gate is a required dependency of check-firefly-protocol-fetch, preserving all
earlier response/stream/same-process fetch checks and existing release/Linux/Windows
workflow entry points. File/service gates and Firefly compilation pass separately
on local macOS. Native Windows/Linux need fresh hosted execution; no GUI window is
opened by this component.

The child is a native test executable, not a live sandboxed WebContent helper.
The product still installs no provider/client/service. Next bind the native channel
bootstrap to trusted live helper/document admission and wire actual navigation/
teardown notifications. Then implement the demanded public protocol/session handler
contract, re-run the pinned Code-OSS entry and follow its real startup blockers.
Navigation, workers, cross-origin resources and full protocol security acceptance
remain separate. This component does not accept a workbench launch.
