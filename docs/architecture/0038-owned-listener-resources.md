# 0038: Release owned native listeners and their abort algorithms

Status: internal macOS ARM64 SDK/helper implementation. Public preload,
contextBridge and IPC remain unsupported.

## Decision

Extend the existing EventTarget compile adapter; do not add a vendor patch or
another translation-unit substitution. Preinclude EventTarget, AbortSignal and
all existing classes declaring event_listener_list_changed before adapting tokens.
At the two successful add/remove notification call sites, preserve the original
virtual call, then report the exact target/listener. The callback runs after the
actual list mutation. Track weak realm/target/listener metadata for owned callbacks.
Repeated JavaScript registration is deduplicated by upstream, while distinct
capture values retain distinct native entries. Explicit remove, once and signal
abort notify the registry through the same native path.

Upstream adds a signal algorithm even when a duplicate listener is not appended.
Track these independently: adapt only its original add_abort_algorithm expression
by mapping the returned Optional ID, preserving the original native closure and
return value. Capture the exact signal/ID and callback realm without inspecting
JavaScript properties. No class layout or algorithm is replaced. Token guards
require two notification calls and one abort registration call. On upstream update,
review both order and the preincluded virtual declarations, not just token counts.

At physical detach, remove owned metadata first, root affected native targets,
listeners and signals, remove the exact signal algorithms and invoke original
remove_an_event_listener for the exact entries. Native target invalidation and
subclass notifications remain upstream. Never clear every listener, abort a signal
or touch an unowned algorithm. Explicit source removal also releases that entry's
recorded signal algorithms. Duplicate algorithm-only entries are released at world
teardown. A logically detached but still running frame can register a listener and
signal; both are included in deferred cleanup. Invalid owners are cleaned at the
native notification/ID boundary.

Do not mutate an AbortSignal's algorithm map during its abort iteration. When its
aborted flag is set, the pinned implementation will finish/clear that map itself
in the synchronous abort steps; remove only our metadata on that path. An aborted
duplicate record can therefore remain weak bookkeeping until world cleanup or
pruning. Counters report cleanup requests, not a census of active algorithms.

## Evidence

SDK tests verify duplicate/capture accounting, exact explicit removal, once and
abort cleanup, GC before native snapshots, removed flags and absence from actual
target lists. Retained native callback references are deliberately invoked after
detach to retain the earlier guard's detached-wrapper/getter protection tests.
A source listener is collected after teardown while its native signal remains
strongly rooted, proving the signal closure no longer retains it. In-flight
reaction/await/generator/microtask/function/getter/proxy cases create listeners and
signal algorithms after logical detach; deferred physical cleanup removes them
and restores context/owner invariants.

The sandboxed helper registers a duplicate source signal listener and other source
listeners beside a page listener with its own signal. Source-only detach removes
four native entries and two native abort algorithms, logged under the matching
renderer PID. Source DOM/getter results stay stable, page delivery continues and
its later abort still works. Self-removing frames and all previous helper/SDK,
ordinary renderer/compositor/process and Firefly/Electron reference gates remain.

## Limits

Event handler attributes, generic WebIDL object operations, script-valued event
fields, other observer types, direct transfers, typed results, workers/finalization,
navigation/BFCache and hard budgets remain incomplete. This does not prove complete
realm reclamation or a general callback membrane. No public preload, new native
platform/package support or full audit is claimed. Ladybird remains unchanged and
no newer pin was tried. Existing linker/deployment warnings and the separate
visible-desktop gate remain.
