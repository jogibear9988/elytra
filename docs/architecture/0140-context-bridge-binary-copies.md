# ADR 0140: Bounded binary copies across contextBridge realms

Status: accepted for the locally tested macOS native/production-helper subset.

The later [ADR 0142](0142-public-raw-binary-ipc.md) adds public application packet
routing and binary IPC; the remaining-transport paragraphs below describe this
decision's original acceptance scope.

## Context

Code-OSS needs Uint8Array messages, including the page/preload boundary before
main-process IPC. Native buffer/view snapshot and raw FFB2 graph support already
exist, but default public snapshots refuse bytes and strict frozen materialization
cannot freeze nonempty typed arrays. Enabling only wire types would not establish
an end-to-end public binary path.

## Decision

Opt contextBridge publication and every call/callback direction into a 64 KiB
aggregate complete-backing-store budget. Keep default BridgeData capture byte-free
and public process IPC unchanged. Reuse native slot-based snapshotting, including
backing deduplication, view offsets/lengths, subtype normalization and no expando/
constructor/getter/iterator execution. Shared/resizable/detached/out-of-bounds
storage remains refused. A small slice of oversized storage still exceeds budget.

Add an explicit FrozenContainers materialization policy: freeze ordinary graph
objects/arrays while leaving independently copied buffers/views writable. Strict
Frozen retains its binary refusal; Mutable retains its existing behavior. Capture
and materialization retain graph/backing aliases without sharing source storage,
transferring or detaching it. Do not claim immutable bytes merely because their
containing API is frozen. Uint8Array subclasses normalize to Uint8Array; no Buffer
global is installed in the sandbox.

Extend guarded Promise settlement to inspect ordinary native binary instance and
prototype descriptors for then, retaining trap-free rejection of polluted getters,
callable thenables and unknown/host/proxy prototypes. New binary prototype class
IDs use the pinned engine ABI; future upstream changes require the usual rebuild
and regression gates. Existing document identity, weak callback registry, pending
budgets, revocation and exception sanitation remain enforced.

## Acceptance

Actual native LibJS/LibWeb tests exercise binary API publication, synchronous and
Promise arguments/results, callbacks in both directions, independent writable
storage, graph/backing aliases, DataView/typed offsets and an exact 65536-byte
backing store with a 65537-byte refusal. They cover getter-free native slot capture,
polluted typed-array prototype then refusal and copied bytes surviving API detach.
Existing raw native view/wire/fresh-process, isolation, callback/job/DOM tests pass.

One focused hidden production-helper batch adds real document/preload binary calls,
Promise results and callbacks across fresh documents. Main only sends a plain-data
trigger and receives plain-data assertions: no process binary IPC claim is made.
The actual Electron 45.2.3 common reference passes byte/offset/cycle/view-alias and
independent-copy behavior. In this fixture Electron copies backing stores separately;
Firefly preserves one backing per graph, with that assertion scoped to Firefly.
Source ownership/quotas are likewise tested separately from Electron behavior.

## Remaining native process transport

Public invoke/send/webContents.send still reject bytes. Their current seam carries
UTF-8 strings; binary must use the existing raw native channel with canonical
current-document/preload authorization, bounded application dispatch, ACK credit,
timeouts, navigation/closure revocation and actual process ownership tests. Do not
add a Base64/JSON-byte workaround to that seam. The channel components already
have raw-frame and real-child evidence; their public integration remains open.
MessagePort handoff/streaming, large-file limits, shared memory, zero-copy and
performance measurements remain required work. Fresh Linux/native Windows
execution and the full unmodified Code-OSS preload/workbench remain unverified.
