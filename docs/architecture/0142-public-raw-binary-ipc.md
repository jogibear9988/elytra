# ADR 0142: Public copied binary IPC on the native document channel

Status: locally accepted macOS invoke/reply and bidirectional send subset. No
Code-OSS workbench, MessagePort, streaming or general platform acceptance claim.

## Context

ContextBridge copies and a real native document/preload byte-channel handoff are
already accepted independently. Application IPC still used a UTF-8 string seam,
which cannot carry raw FFB2 bytes. A second unordered payload route would also risk
reordering ordinary/binary requests and reporting stale native URLs after history
updates. Required sender, quota, revocation and error rules must survive integration.

## Decision

Use the bound raw channel for all application payloads, plain and binary. Append
no new upstream control message or patch. Keep the accepted native epoch, exact
page/document/navigable/realm binding, helper OS PID hello, and frame ACK credit.

Introduce a fixed FIM1 application header: magic/version, direction-specific kind,
success flag, reserved zero bytes, epoch and request ID. Check schema, nonzero IDs,
128 KiB frame bound, direction and epoch before graph handling. Kinds are Invoke,
Send, Reply, Event and private application Ack. The 20-byte header leaves at most
131052 encoded graph bytes. FFB2 contains graph metadata plus raw binary attachments;
never convert attachment bytes through UTF-8, Base64 or JSON number arrays.

Enable the existing 64 KiB distinct-complete-backing policy in public native
capture/decoding. Preserve cycles, view type/offset/length, graph aliases and backing
aliases; normalize Buffer to Uint8Array. Receiver storage is writable and independent.
Shared/resizable/detached/out-of-bounds storage, accessors/proxies, native objects and
broader structured-clone types remain refused. Existing default data-only snapshots
still reject binary unless explicitly opted in.

Main's private JS adapter receives a destination-owned byte view and native metadata
object, not a text wire. Native encode/decode read byte-view slots directly, with
realm/storage/geometry limits and no property hooks. Public ipcMain listeners and
handlers keep their Electron-shaped signatures. Unsupported return values become
sanitized error replies. Old string payload events/replies cannot reopen that path.

## Ordering, admission and lifecycle

A native request marker (empty wire) uses the original control connection, after
history updates. Join it to the matching raw payload and request kind; only after
its history queue step is complete capture the canonical native URL. Invoke/send
markers and raw request IDs are each monotonic. This retains per-direction request
ordering even when the two native transports deliver their halves in either order.
Neither app graph nor marker-supplied URL identifies the sender.

Reserve native per-document 16/global 64 pending credit on the first half and keep
it through history ordering and VM-busy retries. Hold at most 16 refused-half IDs
so the counterpart cannot revive a quota refusal. Missing halves expire after 30
seconds with credit release and a bounded late-counterpart tombstone. Excess/invalid
admission retires the channel instead of growing an unbounded collection.

Early preloads keep their bounded native requests until the actual handle arrives.
Send the native hello first, then flush application frames as byte-channel credit
becomes writable. Queues are capped at 48 unsent frames per endpoint/direction;
underlying transport credit remains 16. Pending public requests remain 16/document
and 64/helper or host. Unsent expired requests/events are discarded when flushed.
No JavaScript VM pointers are retained in native packet queues.

Enter main JS only when its VM is available; recheck native document/request ownership
before dispatch. Build mutable destination graph values, run normal application
listeners/handlers, and release event credit through a private reply. Outgoing
webContents events retain their separate 16/document, 64/host application ACK budget.
A 30-second missing outgoing ACK retires that direction until a fresh document.

In the helper, enter fresh world frames and defer at most one settlement per pending
request while a source frame runs. Defer event dispatch with a bounded incoming count.
Channel failure rejects live pending invocations; a delayed settlement cannot revive
a retired success. Revocation clears queues/roots. Parent epoch checks prevent a
late old close callback from clearing a replacement. Navigation and window/world
teardown reclaim native budgets; effects already started are not rolled back.

## Validation

The native probe checks an independent application-header byte vector, flags/kinds,
zero IDs, truncation and exact/overflow frame sizes. Existing raw channel tests cover
malformed outer frames, unexpected handles, credit, reentry and an actual child
process. Actual-engine isolation tests cover the existing binary view codecs and
private raw-wire extraction without getters/proxy hooks, exact realm refusal and
NUL/high-bit byte preservation.

A focused hidden binary fixture verifies an invoke issued during preload before
handle arrival, asynchronous replies, main Buffer normalization, 64 KiB buffer/view
cycles, aliases/offsets, bidirectional sends, independent mutation and document reload.
A real Electron reference checks the common behavior. Firefly backing-alias semantics
through contextBridge are intentionally asserted separately.

The production-helper channel gate now sends three extra native diagnostic handles
per document: unknown realm, wrong document and a duplicate correct handoff. Each
must close its transferred endpoint without sending a hello/application frame;
none may consume or replace the valid admission. Test observers run only with the
explicit parent-side FIREFLY_IPC_BYTES_DIAGNOSTIC_ADMISSION=1 flag. Ordinary apps
allocate no observer channels and expose no diagnostic binding API to page JS.
The batch also checks live OS parents, three fresh document epochs and two windows.

Complete existing public request/reply regression passes: mutable clones, errors,
concurrent replies, hostile capture, quotas/recovery, stale replies, navigation,
closure, real helper crash/replacement and the actual 30-second request deadline.
The send-event gate preserves early/order/once/native sender and same-document
history behavior. The full outgoing gate includes the actual OS-verified SIGSTOP/
SIGCONT stalled-peer timeout and fresh-document recovery. Native hello-stall fault
injection and broader hostile-compromised-process testing remain open; the ordinary
request deadline/stopped-peer tests do not prove that separate hello timeout path.

Local macOS Release and the scoped real Electron references pass. Linux Xvfb and
macOS aggregate CI include the new binary gate; fresh hosted Linux/native Windows
execution remain unverified. Ladybird and Mite's dependency boundary stay unchanged.
Next implement actual MessagePort handoff/streaming and the other demanded Code-OSS
preload/worker/cache/resource contracts; this is not an editor/workbench acceptance.
