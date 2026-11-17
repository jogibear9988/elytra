# ADR 0209: Reserve application IPC control capacity

Status: implemented; native component and local macOS renderer acceptance, 2026-11-17.

## Problem

Both directions previously counted bulk port DATA as ordinary control packets in
one 48-slot application queue. Several real routes could consume every slot and
leave no capacity for replies or acknowledgements needed to release other work.
Raising quotas postpones that contention. Priority alone also cannot help a
control packet that cannot enter the queue.

## Decision

Use one shared native admission/scheduling policy in the main bridge and renderer
helper. Keep the existing 48 control slots and one extra ordinary main Event:

| Traffic | Nested queue bound |
| --- | --- |
| Bulk: port DATA/fragments, attachments, helper Invoke/Send | 16 |
| Bulk plus sequenced port ACK/CLOSE | 32 |
| All above plus independent Reply/application ACK/attachment ACK | 48 |

The last sixteen slots match genuine critical demand per document. Helper
Invoke/Send share one active count of sixteen, and both are settled by Reply.
Main ordinary Events and port attachments likewise share one sixteen-entry active
window, settled by ACK/PortAck. Forged traffic outside admission remains a channel
failure; reserved slots are not extra authority or speculative consumption credit.

Only independent critical packets may pass unstarted work. Preserve relative
order of every sequenced PortControl; pin an already emitted partial FIM2 logical
message until complete. After sixteen critical completions, give the oldest
noncritical packet a turn if available. Persist that fairness counter across
transport stalls; lack of a preferred class must never stop eligible work.

Preflight an entire port DATA fragment batch before ledger/sequence commit. One
existing wire-byte lease spans commit and every queued fragment; retain it until
the last fragment is sent/canceled. Do not release the lease and re-reserve later.
ACK/CLOSE similarly retain their existing lease. Keep all logical waiting, active,
wire-byte and deadline limits unchanged. Existing bounded JS/helper queues retain
unadmitted work; no application retry loop or Code-OSS patch is introduced.

This application queue is separate from the byte transport's sixteen physical
unacknowledged frames. Real native transport ACKs permit progress without waiting
for author/DOM consumption. A paused peer still cannot receive data. Exhausted
wire budgets and invalid/expired channels can still fail closed; slot reservation
does not establish unconditional delivery or arbitrary saturation tolerance.

## Validation boundary

Review and actual compile-database syntax checks pass. Release Firefly/offscreen
builds and the complete byte-channel gate pass. The new headless native probe uses
this production scheduler, real packet/port codecs, wire leases and paired
transports: 16/32/48 capacity plus Event, whole nine-fragment preflight, sixteen-
completion fairness, actual FIM2 partial-message pinning, sixteen real physically
unacknowledged frames followed by queued bulk/control traffic, real receiver
resume/ACK progress, byte-exact FUP order and lease release/cancellation.

The partial FIM2 test uses Invoke: PortControl is intentionally physical FIM1 with
separate FUP fragmentation and is not a valid outer FIM2 kind. The first fixture
attempt was correctly rejected by the codec; only the fixture kind was corrected.
Windows handle-broker bootstrap matches the existing byte-channel probe, but no
Windows runtime acceptance is claimed.

The actual macOS offscreen renderer-port batch passes all six cases, and the real
binding observer passes all five document handoffs/15 rejected probes. Evidence:
build/physical-control-reserve-{build,renderer-controls,binding-controls}.log.
The original non-diagnostic Code-OSS editor/three timed parent-liveness/real Bash
terminal workflow also passes. Main exits 0; all fourteen observed native process
identities are independently absent with zero cleanup signals. Evidence:
build/codeoss-physical-control-{acceptance.log,evidence.json,workbench.log}.
Fresh platform CI, broader pressure/failure/teardown behavior and unconditional
delivery remain open.
Ladybird stays unmodified.
