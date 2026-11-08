# Files and binary data across processes

Status: current limitations plus prioritized implementation work for Code-OSS.
The future paths below are design requirements, not usable Firefly APIs today.

## Current boundary

Firefly [request/reply and bidirectional event IPC](../api/ipc.md) now copy supported
fixed ArrayBuffer/typed arrays/DataView between page, isolated preload and main.
Buffer values normalize to Uint8Array. The budget is 64 KiB of distinct complete
backing stores per graph, with a 128 KiB native application frame (20 bytes of
header plus the encoded graph). Views keep offsets and backing aliases in Firefly;
recipients own independent writable bytes. ContextBridge freezes plain containers,
not binary storage. Shared/resizable/detached storage, File/Blob, MessagePorts and
transfers remain unsupported. Actual public macOS tests and an Electron common
reference pass; broader platform/performance evidence remains open.

Native payloads use the document-bound byte channel, not JSON number arrays or
Base64. A separate native request marker retains history/sender ordering. Pending
admission, native frame credit and unsent queues are bounded; navigation/closure
revoke the channel. [ADR 0142](../architecture/0142-public-raw-binary-ipc.md) describes
this first copied-buffer process path. It is not yet a large-file streaming API.
No file-transfer throughput/latency, shared-memory or zero-copy claim follows.

An [internal native buffer/view snapshot and wire slice](../architecture/0099-bounded-native-binary-wire.md)
now passes local macOS separate-process tests, with raw binary attachments and
explicit byte budgets. It preserves typed-array/DataView offsets and backing
aliases with one copy of each complete backing store. The later public IPC
integration now uses this raw format through the actual native document channel. Streaming and shared storage remain unfinished.
Full backing storage is visible in a cloned view; use a dedicated range when
limiting exposed bytes. No throughput/latency or zero-copy claim follows from this.

A [private native byte-channel component](../architecture/0100-native-byte-channel.md)
now passes local macOS transport and separate-child tests with raw bytes,
acknowledgement credit and explicit queue limits. Its actual main/WebContent
handoff, canonical document authorization and public bridge integration are
now covered by the later document/preload channel handoff and application packet
integration. Real OS-parent and epoch/document checks, reload/multiwindow rebinding
and native unknown-realm/wrong-document/duplicate-handle refusal are included in
the focused production-helper gate. This still does not establish port/stream APIs.

## What Electron and Code-OSS actually establish

Electron [IPC](https://www.electronjs.org/docs/latest/api/ipc-renderer) uses structured
clone for messages and explicitly rejects browser DOM File objects sent to main.
Its [webUtils.getPathForFile](https://www.electronjs.org/docs/latest/api/web-utils)
returns a disk-backed File's path, not its file contents. Transferring a path is
small metadata; reading the file and copying its bytes are additional operations.
[MessagePorts](https://www.electronjs.org/docs/latest/tutorial/message-ports)
provide another messaging route, not a general zero-copy guarantee.

The [pinned Code-OSS audit](../compatibility/vscode.md) identifies URI/file-provider
commands and binary Uint8Array messages over MessagePorts. Its inspected port send
uses postMessage(message.buffer), without a transfer list. That source establishes
a binary messaging API, not zero-copy file data or a measured low-overhead claim.
Benchmark Electron and Firefly with the same workload before comparing costs.

## Prioritized implementation

1. **Binary snapshots:** support the required byte views across the actual page,
   isolated preload and main-process boundaries. Preserve exact byte ranges and
   isolation; never serialize bytes as JSON/base64. Keep payload/memory/pending
   budgets, hostile input checks and explicit errors. The same-process page/preload
   copy and actual native process IPC milestones now pass locally, including early
   preload queueing, bounded main-VM admission, native history ordering and replies.
   Broaden adversarial runtime coverage and native platform acceptance as the port/
   streaming route is implemented.
   Bounded copied buffers are the first correctness milestone, not the final
   large-file transport.
2. **Ports and streaming:** implement compatible port handoff and binary messages,
   ordered delivery/closure and backpressure. Large reads must deliver bounded
   chunks with cancellation, avoiding whole-file buffering in each process.
3. **File access by reference:** prefer an authorized, document/window-scoped file
   capability or brokered resource route for large disk-backed content. A small
   token/descriptor can identify the data; the renderer reads it lazily. Preserve
   Electron-compatible application patterns where applicable; define any extra
   Firefly API in an ADR. Never give arbitrary paths, filesystem access or raw IPC
   handles to untrusted page content. Validate offsets/lengths and revoke access
   on navigation/window close; handle replacement, symlinks and renderer crashes.
4. **Transfer/shared storage:** evaluate ownership-transfer buffers and native
   shared memory or file mappings where measurements justify them. Define sender
   detachment, mutability, mapping bounds, lifetime and crash cleanup. OS handle
   passing and shared storage do not automatically eliminate all copies or file I/O.
   No universal zero-copy claim before instrumentation and native acceptance.

## Acceptance and measurements

The internal [trusted WebContent handoff](../architecture/0101-trusted-byte-endpoint-handoff.md)
exercises a raw native endpoint in the real headless helper. It is an integration
fixture, not a document-authorized file route or public binary IPC API. Public
messages remain plain data; efficient Code-OSS file/port transport still needs the
ownership, revocation and VM delivery work above.

Require independent content hashes and exact sliced-view bytes across real
processes; cancellation, pressure, navigation revocation, forged/replayed tokens,
out-of-range reads, file replacement and renderer loss must fail safely. Preserve
bounded memory when a peer stalls. Cover native macOS/Linux/Windows independently.

Measure Electron and Firefly at the same source/workload and protocol: many small
messages, 1 MiB and 100 MiB files, and a 1 GiB streamed file. Record warm/cold-cache
conditions, p50/p95 latency, throughput, CPU, per-process peak memory, allocations
and instrumented copy counts. Verify actual PID separation and correctness before
publishing performance numbers. Large-file stress tests belong in dedicated
acceptance/benchmark jobs, not repeatedly on the user's desktop.
