# Files and binary data across processes

Status: current limitations plus prioritized implementation work for Code-OSS.
The future paths below are design requirements, not usable Firefly APIs today.

## Current boundary

Firefly [request/reply IPC](../api/ipc.md) copies bounded plain-data graphs with a
128 KiB encoded request/reply limit. Typed arrays, Buffer, ArrayBuffer, Blob/File,
MessagePorts and transferable objects are unsupported. The isolated contextBridge
has its own capture/publication restrictions; adding a wire type alone is not
end-to-end support. Do not encode a large file as JSON, number arrays or base64 to
work around these limits. No file-transfer performance has been measured yet.

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
   budgets, hostile input checks and explicit errors. Bounded copied buffers are
   the first correctness milestone, not the final large-file transport.
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
