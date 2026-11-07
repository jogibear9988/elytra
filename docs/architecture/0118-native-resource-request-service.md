# ADR 0118: Native URL requests and credit-paced file service

Status: accepted for the local macOS two-process native component. Live WebContent
admission/routing, public protocol handlers and native Linux/Windows acceptance
remain separate work.

## Decision

Add the request direction above the existing bounded native byte channel. Keep
checked files in the main-side FileServer, separate from the pure frame codec and
LibWeb response adapter. C++ reuses the existing native IPC and file ownership
without an additional FFI or parallel transport. Standalone Mite remains outside
this graph; Ladybird stays unmodified.

The trusted native caller must bind a transport and nonzero epoch to one admitted
document before creating the server. An epoch in a packet is correlation, not
authority. For each Get/Head, call the native authorization function with the URL
and HEAD flag. Only its returned path/headers reach FileRequests. The wire carries
no filesystem path, arbitrary headers, JavaScript value or native handle. The
authorization function must enforce the document's route/origin/path policy; the
server does not turn URLs into paths. This synchronous native callback is internal,
not an implementation of Electron's asynchronous application handler contract.

Requests use a distinct FFQP version-1 envelope. Fields are little-endian with no
native-struct layout:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | FFQP magic |
| 4 | 1 | version 1 |
| 5 | 1 | Get=1, Head=2, Credit=3, Cancel=4 |
| 6 | 2 | zero reserved |
| 8 | 8 | native-bound epoch |
| 16 | 4 | nonzero request ID |
| 20 | 4 | exact payload length |
| 24 | 8 | credit bytes, only for Credit |
| 32 | variable | URL bytes, only for Get/Head |

Get/Head requires nonempty valid UTF-8 without NUL, at most 8192 bytes and no
credit. Credit has no payload and grants 1..65536 bytes. Cancel has neither payload
nor credit. Refuse unknown kinds/versions, nonzero reserved bytes, malformed sizes
and zero identities. Response metadata/body/completion/cancellation continues to
use the existing FFRP envelope, without a new copy or JSON body representation.

Consume monotonically increasing IDs even for denied routes. Refuse replay,
foreign epochs, future credit/cancel IDs and more than eight jobs, including
refusals waiting to send. Retired-ID credit/cancel may already be in flight and is
ignored. Reject outstanding credit above 64 KiB. FileRequests still applies its
eight-per-owner/64-per-process checked-file lease budgets.

Send metadata without body credit. Read chunks only with consumer credit, with
HEAD/empty completion as explicit exceptions requiring no body bytes. Round-robin
ready jobs one frame/chunk per deferred task. Stop when the byte channel's actual
send window is full and resume on acknowledgement; do not conflate transport ACKs
with consumer body credit. Metadata is released after sending. Malformed messages,
transport errors and peer loss permanently revoke the server and close its files.
Explicit revocation is idempotent, including from the authorization callback.
Callbacks/timers hold weak owners and cannot revive the service.

Each job has a fixed 30-second lifetime, including denied/completed responses
waiting behind a stalled channel. A native one-second sweep revokes the channel
if any job exceeds that budget. Credit does not extend it. This initial policy
settles all consumers via disconnect and releases every file; independent
per-request timeout errors can be added with the later public handler contract.

## Evidence and limits

check-firefly-protocol-server uses actual parent/child native transports. Only the
parent knows the CLI-authorized real Unicode file path. The child sends URLs,
credits and cancels. It checks all 4 MiB + 19 bytes, metadata, parallel GET/HEAD and
denied file: URLs. No body arrives before consumer credit. Verify cancellation at
one granted chunk, peer disconnect, actual forceful child termination, native
revocation and reentrant authorization revocation. Each terminal owner releases
all jobs/files. Wrong epochs, replay, future credit, over-credit, quota overflow
and malformed packets fail closed. A real paused-file test waits for the native
30-second deadline, verifying that it did not disconnect early. Independent Python
struct/UTF-8/budget controls test request encoding and malformed input rejection.

The existing check-firefly-protocol-files release/Linux/Windows gate now requires
this service gate as well. Byte-channel, real LibWeb/JavaScript fetch and Firefly
compilation remain separate checks. No desktop window is opened by these tests.

The consumer is a native child fixture, not a WebContent document. The server is
not installed in the product and exports no new JavaScript API. Next connect the
LibWeb response owner to this request/credit channel, bind both ends to a live
authorized helper/document and test navigation/revocation/security in the real
renderer. Public protocol/session handlers and the Code-OSS workbench remain open.
