# 0076: Weak lifetimes and owner-thread finalization jobs

Forced-GC tests exposed three integration defects: WeakRef targets could disappear
within their creation job, registries never received cleanup jobs, and observed
composite/timeout AbortSignals could disappear before delivering notification.
Fix these in Mite without modifying the pinned Ladybird sources.

## Host ownership

Keep deduplicated object/symbol roots for WeakRef construction and successful
dereference until the complete nextTick/Promise checkpoint finishes. Then clear
those roots and call LibJS finish_execution_generation. Native WeakRef lookup and
branding remain in the engine; a small authored ESM adapter supplies the host roots.

Replace the VM finalization enqueue hook while EventLoop owns the VM. Collection
only roots/enqueues the registry and signals a libuv async handle; application JS
runs later on the owner thread. Drain a rooted snapshot through normal dispatch
and checkpoints, preserving new work queued by cleanup. Unreference the wakeup
when its batch is taken. Fatal shutdown discards pending application cleanup and
restores the original VM hook before native teardown.

The pinned engine traces unregister tokens strongly. Map public tokens weakly to
independent native group tokens in an ESM FinalizationRegistry adapter. A native
group has no back-reference to its public token. This preserves grouped unregister
and permits target-as-token registrations to be collected. Native target/held-value
validation and callback execution remain authoritative.

## Relative signal retention

A reachable source retains a composite while it has abort listeners, an onabort
handler or private consumer algorithms. Otherwise dependency links are weak.
Composite state retains its sources; unreachable cycles have no global strong root.
Track actual listener identity/capture and once/removal/guard cancellation so the
retention reflects dispatch state rather than historical listener registration.

Timeout callbacks own a weak slot, made strong only while observed or required by
a dependent. Separate closure scopes avoid accidentally capturing the signal.
Cleanup holdings contain weak source links or native timer handles, never their
target. Explicit abort unregisters cleanup, clears native timers and removes links.
Listener guards unsubscribe through private algorithms despite propagation stops.

## Evidence and limits

Twelve headless embedded-host fixtures compare actual Node 24.15.0 behavior under
forced GC: same-job object/symbol roots, later collection, asynchronous cleanup and
nested jobs, fatal cleanup/shutdown, weak grouped tokens, listener-only composition,
listener removal/once/attributes/guards, deadline delivery and timeout release.
The test-only host also counts active native libuv timers. No desktop is launched.

This is a tested lifetime subset, not a deterministic GC schedule or complete
Node/Web conformance claim. Keep explicit close/abort as the primary resource
contract. Native platform gates and hosted Firefly acceptance remain separate.
See [the cancellation contract](../api/abort.md) and
[async ownership](../concepts/async-and-lifetime.md).

## Nested composition follow-up

Connect any() directly to the distinct original sources of its inputs. This avoids
needing a recursive strong-retention walk and lets intermediate composites die
while original sources retain observed leaves. Empty composites contribute no
sources. Keep the full input validation and first-already-aborted selection before
building flattened links.

Actual Node 24.15.0 loses listener-only leaves when an intermediate's cleanup
removes its flattened source registration. Keep a dedicated assertion for that
reference defect and Mite's correct delivery rather than copying the failure.
Additional shared tests cover diamond/deduplicated inputs, collected intermediate
signals and reachable leaves. The reference implementation is
[Node's pinned abort controller](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/abort_controller.js).
