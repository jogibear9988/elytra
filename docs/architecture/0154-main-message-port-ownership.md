# ADR 0154: Main MessagePort ownership core before native handoff

Status: accepted as a locally tested main-realm component; cross-process ports remain unfinished.

## Context

The actual prepared Code-OSS graph needs MessageChannelMain. Its Protocol consumes
Uint8Array frames through EventEmitter message events and explicitly calls start.
Its utilityProcess.connect then transfers an endpoint to a utility process, and
window paths transfer further endpoints through webContents/ipcRenderer. A constructor
that merely bypasses the import cannot satisfy those workflows. Implement the real
ownership/queue core, then bind it to genuine native processes/documents without
changing the public API or substituting serializable application-controlled IDs.

## Decision

Create connected EventEmitter main endpoints with native BridgeData snapshots,
start-gated FIFO jobs, binary aliases, endpoint transfer, detachment and generation
checks. Reuse the actual public binary codec/limits; no JSON fallback or Node/V8
runtime is introduced. MessagePortMain remains a returned type, not an exported
constructor. Export MessageChannelMain from the actual Firefly/Electron-alias host.

Keep wrapper authority in a WeakMap. A weak canonical record table plus scalar
queue leases allows cycles of unreachable queued transferred wrappers to collect.
GC closes surviving peers and reclaims endpoint/wire budgets. Transfer changes the
owner/generation, retains accepted incoming data and resets start. Old wrappers and
stale callbacks cannot continue to address the moved endpoint.

Validate data, transfer rights and quota admission before mutation. Review catches
that inherited emitter construction can throw or reenter: preallocate all receiver
wrappers/weak references/tokens and revalidate snapshots/admission before commit.
Capture registration primitives and use own-slot appends so the tested Array.push
mutation cannot throw after detachment. Recheck shutdown after constructor reentry.
This protects these tested operations, not arbitrary privileged main global tampering.

Close drops incoming work and undelivered transfer ownership, but preserves accepted
peer messages before its close marker. Local close notification is deferred (an
explicit timing difference from Electron); retain its owner until that notification
runs. A discriminating GC regression shows weak-only notification loses the callback,
while the retained-owner implementation delivers once. App shutdown clears all queues
and prevents new channels. Bound live endpoints, per-endpoint/global encoded bytes,
queued messages and transfer counts; unsupported graphs/transfers fail explicitly.

Do not fabricate native handoff: webContents.postMessage explicitly refuses and
renderer/utility process ports are unavailable. The next binding must preserve actual
process/connection/document identity, revocation, transfer commit and binary framing.
The main-realm core is a step toward that required end state, not a replacement for it.

## Evidence and remaining work

Real Firefly/Electron public tests pass data/cycle/binary snapshots, FIFO/start,
repeated transfer, detached-owner behavior, clone/quota atomicity and close ordering.
A new genuine headless Mite/native BridgeData probe forces GC, fills all 256 slots
with unreachable queued-transfer cycles, proves endpoint/byte reclamation and tests
snapshot survival, dropped transfers, inherited hooks and shutdown/close reentry.
No mock codec or desktop input automation is involved. The unchanged pinned Code-OSS
Protocol and once helper pass with real compiled base services and public channels.

The rebuilt actual ready/main graph advances from missing MessageChannelMain to
missing utilityProcess. No editor/workbench/open-edit-save, utility/shared process,
renderer/DOM port or performance acceptance follows. Native Linux/Windows execution
is still unverified. Wire both native lifetime and public gates into check-firefly
and hosted Linux gates, and require their actual results before claiming platforms.
Continue genuine utility parentPort and renderer port transfer, then remaining
workbench/window/preload/file/extension/PTY/native-platform goals. Ladybird is
unmodified and Mite remains independent. See [API scope](../api/message-channel-main.md).
