# 0040: Isolate event handler storage by registered realm

Status: internal macOS ARM64 SDK/helper slice tested. Public preload/contextBridge/
IPC and complete event-field/observer containment remain unsupported.

## Decision

Keep the vendored checkout and native class layouts unchanged. Extend our existing
EventTarget build adapter with exact, count-checked seams: four handler-map address
expressions, native target GC tracing, handler listener realm selection, and native
scope entry for document attribute handling/full erasure. All other upstream bytes
are preserved. The generator test reverses every declared edit and compares the
complete source byte-for-byte; missing/duplicate entry or storage seams fail.
No upstream algorithm copy or patch file is committed. This is adapted engine code,
not an unmodified binary; candidate updates require semantic and runtime review.

WorldJobGuard owns native map containers keyed by weak registered realm and native
EventTarget. Ordinary property getter/setter/deactivation use the current realm's
map. Unregistered contexts delegate to upstream principal storage. Each native
EventTarget traces all its private map values during visit_edges. The native
registry itself roots neither target nor realm, so a detached target/callback cycle
can collect. Prune dead records before every private map lookup. Do not replace
this tracing arrangement with strong roots in the native metadata registry.

An isolated getter therefore returns its own callback or null, never the principal
or another isolated world's callback. Native upstream activation/deactivation still
provides identity, callback replacement ordering, strict return behavior, target
reflection and real event-listener registration. Choose the native handler-listener
realm from its storage owner, even if the assigned callback belongs to a foreign
realm. The callback's associated realm remains unchanged for actual invocation.
Otherwise dispatch could select the foreign realm's handler map accidentally.

HTML text event attributes remain document behavior. A scoped per-VM principal
policy around element_event_handler_attribute_changed keeps setAttribute/remove-
Attribute in principal storage even when called from isolated code; compilation
continues in the original document realm. This policy restores on every return.
Full native target erasure clears every owned map for that target, then uses the
principal policy for the original principal-map/listener erasure.

Physical world teardown removes that world's native listener registrations and
map metadata, roots affected targets and all handler cells through cleanup, and
uses a scoped exact-map override to call the original null setter/deactivation.
It never clears a different world's map. Late map writes inside an executing,
logically detached frame are included when physical detach occurs. A direct call
into an already revoked realm must not fall back to principal storage: it gets a
private map scheduled for rooted native microtask cleanup after the call returns.

## Evidence

All acceptance gates pass in build/handler-storage-acceptance-test.log: SDK,
sandboxed helper, ordinary renderer/compositor/process and Firefly/Electron reference.
SDK tests use a page and two isolated worlds on the same target, verify own getter
identity/prototypes, null foreign getters, unchanged page callback, replacement
ordering, inline attribute principal execution and independent null setters/detach.
GC precedes callback dispatch; a detached node/handler closure cycle collects while
its world remains alive. Full native erasure invoked from source clears principal
and both private maps, and new handlers can rearm. A foreign-realm native callback
still dispatches from the storage owner's map and stops when that owner detaches.
Retained handler references are explicitly sent through the native gate after
cleanup to preserve the detached-wrapper rejection control from ADR 0039.

The real helper checks source getter absence of existing page handlers, source
callback identity/prototypes, unchanged page getters, actual delivery/cancelation,
page handler/listener continuity after source detach, no source errors in page
events, map cleanup counters from the matching renderer PID, and a self-removing
frame handler. Native beforeunload conversion/lifetime and all previous isolation/
listener/signal/observer/timer controls remain passing. Additional SDK checks pass
in build/handler-storage-late-write-test.log: reaction/await/generator/microtask/
listener/getter/proxy frames create private handler maps after logical detach and
require their physical release. A retained raw C++ harness call after physical
detach writes only private storage, immediately loses its native registration and
releases the late map at a native checkpoint. No raw-call page API is exposed.

## Limits

The map count includes empty maps created by getters; cleanup counters count maps
released, not active callbacks. Dead target metadata is pruned on lookup/teardown.
Additional native map memory is not yet charged to upstream external-memory
accounting; hard budgets remain open. This is registered-realm storage, not a claim
for arbitrary unregistered extension worlds or direct raw-function transfer APIs.
Source cannot obtain foreign handler references through these ordinary property
getters, but other script-valued event fields and generic object operations still
need a membrane. Additional observer kinds, typed results, worker/finalization,
navigation/BFCache and hard CPU/queue/memory limits remain incomplete. No complete
reclamation/security audit, new native Windows/Linux/package or visible-desktop
support is claimed. No newer Ladybird pin was tried. Existing linker/deployment
warnings and the separate locked-desktop visible gate remain.
