# ADR 0141: Native document-bound preload byte-channel handoff

Status: accepted for locally tested macOS handle handoff/readiness only. Public
application packet routing and binary IPC remain unimplemented.

The later [ADR 0142](0142-public-raw-binary-ipc.md) adds public application packet
routing and binary IPC; the remaining-transport paragraphs below describe this
decision's original acceptance scope.

## Context

Native raw byte transport and contextBridge binary copies already work separately.
The process IPC still transports strings and refuses bytes. A byte handle must
belong to the actual configured top-level preload document before it can carry
application messages; origin/URL equivalence or page-supplied identifiers are not
permission. File-resource handles have a different authority and lifecycle.

## Decision

Append a separate firefly_connect_ipc NotRouted method in the owned generated
WebContent control endpoint. Preserve all 251 upstream message IDs and the three
previous owned IDs; do not change upstream source or generator. Use the canonical
staged bootstrap declaration consistently, avoiding duplicate source/staged class
definitions. Keep the callback in the shared native IPC library instance.

After the existing canonical native preload Bind, create a native transport pair
and retain the host endpoint in its window/document state. Send the other handle
on that page's actual WebContent client connection with native page, navigable,
document, realm and monotonic epoch fields. There is at most one admission attempt
per bound document; failures/duplicates cannot establish a replacement until a
fresh document. No handle, epoch or native binding API is published to JavaScript.

The receiver uses the selected server stub's ConnectionFromClient and PageHost to
find the actual page, then the existing configured realm registry. Require the
fully active exact document, its actual top-level local navigable and page pointer,
matching native IDs and exact realm. Invalid/unknown/duplicate handoffs close their
transferred handle. Keep a weak endpoint callback, not another document root.

The receiving endpoint sends a fixed 52-byte raw FIH1 hello containing epoch, real
OS PID, document and navigable fields. Host checks exact size/version/nonzero
fields, current canonical binding/epoch and its actual owned helper PID before
marking readiness. Wrong/repeated frames retire the channel. Existing raw-channel
sequence, ACK credit, frame/queue limits and attachment refusal remain in force.
The readiness packet is a protocol proof, not app-provided sender authority.

Navigation/reset/crash/window teardown close the old channel. Epoch checks keep
late close callbacks from clearing a replacement channel. Isolated-world revoke
closes the helper endpoint and session shutdown unregisters the control callback.
A 30-second missing-hello timeout retires the native endpoint until a fresh document.
No application payload receiver or echo service is installed: unexpected payloads
are refused until the application protocol is implemented.

## Evidence and scope

A native non-GUI probe checks an independent little-endian byte vector containing
NUL/high-bit values, exact identities and every truncation, zero field, wrong magic
and oversized frame. The existing raw channel/fresh-child/adversarial/credit suite
also passes. The owned generator preserves all upstream and previous owned IDs.

A focused production-helper test uses two hidden windows, reloads the first and
expects three distinct document readiness records with increasing epochs. It
checks every observed helper PID against its live OS parent and runs the existing
public IPC/contextBridge assertions and window teardown in that same batch.
Diagnostic identity records require FIREFLY_IPC_BYTES_DIAGNOSTIC=1 and contain no
application values. Default application execution produces no readiness log.

Local macOS Release build and targeted acceptance pass. The native unit and macOS
aggregate/Linux Xvfb gates are wired; fresh hosted Linux and native Windows are not
verified. Actual forged/duplicate handoffs and stalled hello retirement still need
adversarial runtime acceptance before public binary API admission. Do not substitute
the pure frame schema probe for those lifecycle/authorization tests.

## Next application routing

Implement native application packet kinds and raw FFB2 graph payloads on this
bound channel, with bounded early preload queues, canonical sender metadata,
main-VM busy/history ordering, request/reply identity, ACK credit, timeouts and
navigation cancellation. Native encode/decode must keep raw storage out of UTF-8
string conversion. Default process IPC still rejects binary; MessagePorts,
streaming, shared memory, zero-copy and full Code-OSS startup remain open.
