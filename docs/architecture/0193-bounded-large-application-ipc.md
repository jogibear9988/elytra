# ADR 0193: Bounded large application configuration messages

Status: implemented source; fresh component and actual application acceptance are
recorded separately in PROGRESS.md.

## Demand

Pinned Code-OSS sends its entire NLS catalog in the real window configuration:
34,273 strings, 1,708,956 UTF-16 text units, plus 160,255 units for array keys.
Its staged JSON file is 1,814,024 bytes. The existing graph representation encodes
UTF-16 units as JSON integers, making the wire substantially larger. The old
4,096-value/65,536-unit/128-KiB application bounds cannot carry this configuration.
Changing only prototype handling therefore leaves startup blocked. Configuration
also crosses an asynchronous contextBridge method after IPC decoding.

## Decision

Keep the existing C++/LibIPC transport and its 128-KiB physical packets. Add a
native application-message codec above it. Small messages retain FIM1 framing;
FIM2 fragments carry fixed kind, success, epoch, request, total size and offset.
Receive only contiguous chunks with identical identity, no interleaving, restart,
gaps or overlap. Validate declared total before allocation and dispatch only a
complete message. A fixed first-fragment deadline does not renew on progress.
Close/timeout/revocation clears state; a partially sent message cannot be skipped.

Queues retain one logical-message cursor and refill through the existing sixteen
native credits. Logical count remains bounded at48 per document/direction. RAII
byte reservations follow buffers through partial reception, queued delivery and
deferred VM entry:16MiB combined per document and64MiB per process. Native ACKs and
request/history markers retain their original meaning; there is one marker per
logical application request, not per physical fragment. Wire graph payloads remain
bounded at16MiB and binary backing stores at1MiB per application graph.

Use an explicit application wire profile:65,536 values,131,072 properties,
4Mi UTF-16 units including keys, depth64,65,536 elements in any array and131,072
cumulative array lengths. Default wire/utility/main-only MessagePort policies do
not change. Graph capture now also checks cumulative array lengths before native
materialization, avoiding sparse-array allocation multiplication.

ContextBridge has a separate capture-only size profile sufficient for the same
catalog, retaining its64-KiB binary budget and the same authority/type rules.
Ordinary class-instance data is admitted only by IPC capture; API publication,
source-realm checks, native identity redaction, trusted getter policy, function
ownership, frozen containers, pending quotas and revocation remain in force.
Its synchronous snapshots are not native wire buffers and do not inherit the
wire queue's aggregate reservation guarantee.

## Consequences and limits

This is copied, bounded data, not file streaming, MessagePort transfer, shared
memory or zero-copy. The existing JSON graph has substantial overhead for large
text; compact graph metadata is a future performance improvement. Byte quotas
cover retained serialized buffers, not all parser/VM allocations or JS-owned data.

C++ fits the existing owned codec, RAII transport and LibJS adapters without a new
FFI boundary. Ladybird remains unmodified. Native codec tests cover independent
framing vectors, malformed transitions and quota ownership. A focused headless
mode reads the actual staged catalog, compares every round-tripped translation,
and checks its real asynchronous contextBridge path. These component checks do
not prove an editor/workbench run; that requires the actual pinned application.
