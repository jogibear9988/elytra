# ADR 0151: Bounded native Node-style zlib streams

Status: locally tested partial runtime API and real Code-OSS compression component.

## Context

The actual ready/main module graph now needs node:zlib. Code-OSS's IPC layer creates
raw inflater/deflater streams, preserves context across messages and uses ordered
flush callbacks with RFC7692 trailer removal/reinsertion. Accepting imports without
real stateful compression would fail the next actual workflow. Existing Fetch/Web
Streams inflation deliberately has a different framing/termination contract.

## Decision

Add a separate small C++ Codec host using the existing pinned native zlib library.
RAII owns native streams; closure-held shared state outlives wrappers without JS
pointers crossing native calls. Copy at most 1 MiB input and one bounded output
chunk per step. Clear all native byte pointers before any JS allocation/return.
Validate internal integer/byte bounds and detached buffers. Close handles explicitly
on destruction/error and natively when unreachable state is collected.

Use an authored ESM options policy and Transform facade. The loader injects stream,
Buffer, native handles and real scheduling functions; Mite remains independent of
Firefly/LibWeb. Register a shared zlib builtin and reproduce its embedded source
with the existing pinned build tools. Public streams retain pending transform work
while readable push returns false, then resume from read demand. Yield native steps
via setImmediate so repeated output does not monopolize nextTick processing.
Private empty Buffer markers serialize flushes with data in the writable queue.
User callback/data-handler exceptions are not recast as native codec errors.

Implement raw/zlib/gzip encode/decode streams and sync/error-first convenience
functions, with output caps. Retain gzip member/padding validation. Keep unavailable
flush kinds, dictionaries, reset/params, autodetection, Brotli/Zstd and crc32 explicit.
Do not claim full Node option/coercion/errno/threadpool/event parity. Document the
observed cancellation difference: Mite retires an interrupted pressure-blocked write
callback once, while Node 24.15.0 retains it. Reject emitClose:false rather than
promise equivalent close-callback semantics. See the [API contract](../api/zlib.md).

## Evidence and remaining work

Local Node/Mite/Python roundtrips verify real gzip/zlib/raw wire formats, fragmented
headers/chunks, stateful message context, convenience callbacks, corruption/truncation
and output caps. Native-host forced GC, readable pressure, close and cancellation
pass with explicit Node differences. Pin the actual Code-OSS IPC source, extract
its unchanged compression classes, and run them with the real compiled base services
on Node and Mite; multiple context-sharing messages pass. This is a component probe,
not a mocked desktop or an accepted application workflow.

The rebuilt Firefly actual ready/main graph advances to missing node:assert. No
editor window, open/edit/save, complete webview/remote, platform-wide support or
performance result is claimed. Ladybird remains unmodified. Continue the genuine
assert contract and real startup, then window/preload/port/file integration. Keep
full original framework/platform/extension/PTY requirements active.
