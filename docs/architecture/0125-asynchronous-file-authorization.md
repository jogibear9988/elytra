# ADR 0125: Reserve file requests before asynchronous authorization

Status: implemented native component with local macOS Release separate-process and
production-helper headless acceptance. Public protocol/session handlers remain open.

## Problem

The actual pinned Code-OSS ProtocolMainService uses defaultSession.protocol's
registerFileProtocol callback to authorize workbench resources. The previous native
FileServer required a synchronous authorized pathname. Calling application JavaScript
from that transport callback would also conflict with main-VM event-loop ownership.
A queued native-to-main request needs a completion which may arrive later, out of
order, more than once, or after the document has disappeared.

## Decision

Add FileServer.create_async with an owned request ID/URL/method record, a native
completion and optional authorization-cancellation notification. Keep create as a
synchronous adapter over that one state machine. No JavaScript, GUI, LibWeb or Node
dependency is introduced into the file service. The trusted native caller still
binds its channel/epoch to the intended navigation/document before construction.

Split FileRequests admission from authorization. Admission consumes the monotonic
request ID and reserves an existing process lease before application code runs.
Pending authorization therefore counts toward eight requests per document and 64
per process, while opening no file. Authorization can complete admitted IDs out of
order; it preserves the stored HEAD flag and original fixed 30-second admission
deadline. Keep begin as the synchronous wrapper. Failed opens/metadata, cancellation,
expiry, revocation and destruction release leases and any checked descriptors.

FileServer inserts its waiting job before invoking the authorizer and owns the URL
storage across callback return. Do not emit headers or read files while authorization
is pending; credit can accumulate only within its existing bound. Completion runs on
the owner thread, checks current service/job/waiting/deadline state, then opens the
native-authorized file. A duplicate or stale completion returns false without opening
a path. Negative results use the existing refusal frame. Transport framing is unchanged.

Retain the callback holder through reentry. Cancellation marks the job terminal
before notifying its authorizer. Revocation invalidates weak completions and clears
all jobs/files before notifications; even a callback invoked during owner destruction
cannot obtain a new strong reference to the dying server. The existing real timer
also bounds unanswered authorizations. The service never extends a deadline because
an application callback eventually answered.

## Evidence and limits

The real separate-process service gate adds delayed success, duplicate completion,
reverse-order GET/HEAD/refusal, cancellation before authorization, explicit revocation,
owner destruction, force-killed peer, pending quota overflow and real unanswered
30-second deadline modes. Cancellation notifications deliberately call their stale
completion reentrantly; every terminal result is refused. Old body/credit/frame/
replay/quota/cancellation/deadline tests continue to pass.

The file-owner gate checks reserved-but-unopened jobs, out-of-order IDs/methods,
duplicate/cancel/deadline refusal and shared 64-request reservation/release. Existing
file descriptors, metadata, mutation and concurrent quota tests remain required.
The real production-helper resource probe now defers authorization to another native
task before returning its fixed file route. External ESM/CSS/binary/HEAD and document
identity/navigation-teardown controls pass locally without desktop windows.

Existing release and Linux/Windows file-service gates include the new modes. Fresh
native hosted execution remains required, and no Windows production-helper support
is claimed. This is not public registerFileProtocol/session support: next connect
main-side registration, queued request/reply ownership and automatic native grants.
Code-OSS's actual main import still stops at missing crashReporter; no desktop launch
or application callback execution is accepted by this native component gate.
