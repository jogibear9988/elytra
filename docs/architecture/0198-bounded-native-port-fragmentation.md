# ADR 0198: Bounded logical DATA on native MessagePorts

Status: accepted for the locally tested macOS native/offscreen subset.

## Context

The actual Code-OSS extension handshake sends a 170,933-byte encoded graph after
system-extension scanning. The port graph codec can represent it, but one native
frame cannot. Applications need the same MessagePort API and independent binary
snapshots across main, renderer and utility processes. Increasing physical frame
or per-route credit limits would not supply a bounded streaming protocol.

## Decision

Keep the existing small FUP1 DATA envelope byte-identical. Add native-only kind 5
for larger DATA, with big-endian serial, total encoded size and contiguous offset
followed by nonempty bytes. Use one common physical ceiling of 128 KiB minus the
outer document header (131,052 bytes); compile-time assertions bind the renderer
adapters to that ceiling. Transfer attachments, close and acknowledgment frames
remain small. JavaScript sees only complete ordinary DATA envelopes.

Limit one encoded logical graph to 1 MiB, including metadata. Binary graph capture
also charges distinct complete backing stores, so the maximum binary payload can
be smaller than that encoded bound. Keep 16 unconsumed messages and 1 MiB per
route/direction, and 4 MiB per endpoint/direction. Reserve the entire logical receive
quota before allocating its buffer. A native reassembler checks live route and
generation, monotonic serial, total and exact consecutive offsets. No partial graph
is decoded or delivered. A fixed 30-second first-fragment deadline cannot renew;
invalid input, timeout or channel loss cancels partial state. Route teardown drops
its state and refunds the existing ledger reservation.

Preflight the complete bounded physical batch before sending its first fragment:
at most nine frames, existing queue/write slots and retained-byte budgets. Reserve
logical send credit once, then encode one physical chunk at a time. A mid-batch
transport error retires the channel. Physical write/byte acknowledgments advance
transport capacity only; actual MessagePort consumption or successful downstream
logical admission generates the single FIFO size acknowledgment. Close remains
after earlier admitted data; a peer close interrupting an incomplete logical
message is invalid. Local cancellation may discard queued work as before.

Utility channels use an unreferenced native timer to enforce partial deadlines;
it never keeps an otherwise finished process alive. Qt and helper endpoints use
their existing expiry scheduling. Revoke/EOF/shutdown cancel timers, reservations
and buffers. The standalone Mite runtime gains no dependency on this Firefly code,
and Ladybird remains unmodified.

## Validation and limits

The native protocol probe covers small DATA stability, multi-frame exact bytes,
malformed metadata/offsets/replays, route and directional quotas, fixed deadlines
and cleanup. Public fixtures cover local snapshots, 512 KiB utility duplex and an
actual principal-DOM/main/utility relay with sliced backing stores, aliases,
mutation isolation, delayed start and ordered close. Actual Release builds,
protocol probe, native codec/GC gate, main-only public ports, utility duplex and
offscreen DOM relay all pass locally. These gates do not establish other platform
support or usable Code-OSS editor acceptance.

This does not implement transfer lists inside native DATA messages, shared memory,
zero-copy file transfer, unlimited messages, full structured-clone types or
Electron-wide MessagePort compatibility. Fresh Linux/Windows acceptance and real
Code-OSS activation/open-edit-save remain separate gates.
