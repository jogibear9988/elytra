# ADR 0115: Document-owned native file requests and correlated frames

Status: accepted for the local macOS native component. Actual LibWeb request
binding and native Linux/Windows component execution remain separate gates.

## Context and language

The file-body and metadata components can deliver a file to a native child, but
the original diagnostic stream had a single implicit request. A renderer resource
service needs bounded jobs, cancellation, stale-callback protection and explicit
correlation. Keep this owner in C++ beside existing LibCore files and transport;
there is no new FFI, VM dependency or change to Ladybird. Mite stays independent.

## Ownership

FileRequests is a thread-confined native owner for one admitted document generation
(epoch). Its nonzero epoch is supplied by trusted native document code, not derived
from a wire field. An epoch/request pair identifies a job; it does not authorize a
peer or a filesystem path. The caller must already have admitted the real document
and authorized the application handler's returned path. Real WebContent/page
binding, session dispatch and that policy are not implemented by this owner.

Each owner permits eight pending jobs; a process-wide atomic lease pool permits
64 across all owner threads. Lease lifetime follows the actual map record; file
ownership is destroyed before its lease returns. Open, metadata validation,
encoded-head budget and map insertion failures return credit through RAII.

IDs are nonzero monotonically increasing 32-bit numbers for that owner. An admitted
attempt consumes its ID even when later opening or budgeting fails. Completed,
cancelled, failed and capacity-refused IDs cannot be reused for delayed callbacks.
Exhaustion does not wrap. Pulls carry the owner's epoch and return explicit offset,
raw bytes and completion; invalid credit does not advance the file. Completion,
HEAD, cancellation, read failure, expiry, revocation and destruction release jobs.

A job has a fixed 30-second deadline from admission; activity does not extend it.
Pull checks the deadline before IO. expire(now) returns timed-out IDs for the caller
to settle and releases their jobs; it allocates settlement storage before mutating
ownership. Native event-loop scheduling and promise/response settlement still need
the renderer adapter. No timer is installed here, and synchronous filesystem IO is
not forcibly interrupted. Native callers supply MonotonicTime, enabling deterministic
boundary tests without a 30-second sleep or a page-controlled clock.

revoke() permanently closes an owner. Navigation/disconnect integration must revoke
the old owner, then create a separate owner for the new admitted document. Retained
weak callbacks cannot revive it. Bytes already handed to a transport cannot be
unsent; receiver-side cancellation/discard and native document lifecycle wiring
are still required.

## Frame format

FilePacket wraps response metadata, chunks, completion and cancellation in a fixed
32-byte little-endian header. It does not serialize a native C++ struct or path.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | FFRP magic |
| 4 | 1 | Version 1 |
| 5 | 1 | Kind: metadata 1, chunk 2, complete 3, cancel 4 |
| 6 | 2 | Reserved zero |
| 8 | 8 | Nonzero native document epoch |
| 16 | 4 | Nonzero request ID |
| 20 | 4 | Exact payload size |
| 24 | 8 | Body offset |

Metadata has offset zero and a nonempty payload up to the response-head budget.
Chunks are nonempty, at most 64 KiB, and cannot exceed the file-size boundary.
Complete has no payload and names the final position. Cancel has no payload and
zero offset. Reject unknown kinds/versions, nonzero reserved fields, zero IDs,
truncation/trailing bytes, overflow and kind-specific budget violations. A decoded
payload borrows its input only. Frame validation does not validate embedded JSON;
the metadata decoder and request consumer must also check schema/identity/state.

## Evidence and remaining integration

The actual native parent now reads through FileRequests and sends these frames to
a separate native child, which checks epoch/ID, metadata-first ordering, offsets,
all file bytes and final length. HEAD and partial cancellation use the same real
process boundary. Forcefully terminating that actual child after the first 64-KiB
chunk triggers peer-close revocation while its native file job is still active.
Parent cleanup also revokes the fixture's owner. This is
an owned diagnostic consumer, not a LibWeb fetch or application protocol handler.

Native component checks exercise foreign epochs, duplicate/stale/wrapping IDs,
interleaved pulls, completion/cancel/error/metadata-failure/destructor cleanup,
fixed deadline boundaries, expiry results, permanent revocation and late weak
callbacks. Twelve actual native threads contend for 64 shared credits and retain
them until admission is measured. Independent Python struct encoding/decoding
checks frame layout/endian, bounds, kinds, flags, offsets and malformed inputs.
These checks are added to the existing required non-GUI file component gate.

Next implement the real LibWeb pending-response/abort adapter and native
helper/document admission, bounded request dispatch and receiver multiplexing,
application file-handler/session integration and error settlement. Then require
actual navigation/subresource/fetch/MIME/security tests and the pinned Code-OSS
workbench. Public protocol, multi-request renderer delivery, ranges/redirects and
browser response security are not accepted by this component result.
