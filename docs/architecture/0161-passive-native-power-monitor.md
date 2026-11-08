# Passive native power monitoring

Status: implemented for macOS queries and passive observer ownership; actual
physical-transition delivery remains unaccepted. Other native platforms fail
explicitly. Mite remains independent of Firefly and Ladybird stays unmodified.

## Decision

Use native CoreGraphics idle/session information, Foundation thermal state and
IOKit power-source/CPU-limit queries. Missing lock/session metadata yields unknown;
missing power data never becomes AC or a fabricated full-speed value. Maintain a
small platform provider separate from the LibJS dispatcher and ESM policy.

Observe AppKit workspace/session, distributed lock, Foundation thermal and IOKit
power changes passively. Do not register sleep acknowledgement hooks, create
assertions, change settings or synthesize activity. Shutdown observation is passive:
preventDefault explicitly fails. Sleep inhibition belongs to powerSaveBlocker.

## Ownership

Cache validated capabilities once per host. Supported pre-ready subscriptions defer
observation until readiness; first/last listeners start/stop one epoch. Monotonic
bounded epochs retire before native cleanup, so late callbacks and stale stops
cannot affect replacements. Failed starts roll back listeners. Bound synchronous
listener reentry and clear subscriptions on exhaustion; later fresh admission works.
Exit revokes events even if native cleanup unexpectedly fails.

Native callbacks carry plain records and weak state, never JS objects or GC roots.
Queue to the Qt owner through a mutex-protected receiver. Validate current-epoch
payloads and discard unadvertised native event families. Provider stop releases
owned observers/sources on the main thread; native teardown precedes VM retirement.

## Evidence

Production policy passes in actual Node/Mite with controlled peers. The exact native
dispatcher passes controlled worker/epoch/error/teardown tests. Genuine native values
are compared with framework queries, and the production ESM factory/native host/OS
provider component passes read-only queries, passive subscriptions across two epochs
and real GC in a minimal ready context. These do not establish actual desktop
lifecycle or physical sleep, lock, battery, thermal, session and shutdown delivery.

The real pinned Code-OSS local-desktop/NLS entry passes this import and next fails
linking powerSaveBlocker. There is no accepted Code-OSS workbench or open/edit/save.

See the [public contract](../api/power-monitor.md) for supported APIs, differences,
error conditions, test sources and remaining platform acceptance.
