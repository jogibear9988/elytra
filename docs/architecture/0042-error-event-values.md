# 0042: Protect ErrorEvent values through getters and onerror arguments

Status: internal macOS ARM64 SDK/helper slice tested. Public preload/contextBridge/
IPC and a complete event/callback membrane remain unsupported.

## Decision

Generalize the existing EventValues registry to DOM::Event. CustomEvent and
ErrorEvent each have one script-valued payload slot and share the same provenance,
bounded snapshot/cache, tracing and revocation machinery. Add native/generated
ErrorEvent compile adapters for constructor registration, visit_edges, JS constructor
writer and the error getter. The existing EventTarget adapter additionally passes
the special onerror fifth argument through the same reader in the callback realm.
Protecting the getter alone would leave this raw native access path open.

Preserve native event layout/storage and handler processing. Exact source counts,
generation dependencies and reverse-edit preservation tests cover both new TUs and
the special argument seam. The original vendor/generated files are unchanged;
compiled behavior is adapted. No newer upstream pin was tested.

Writing provenance must be explicit, not inherited from an arbitrary native call
stack. Native event constructors register with unknown origin. JS constructor
bindings record their realm immediately after native construction; successful
CustomEvent.initCustomEvent records its current JS entry realm. A native page error
can be reported while a Source dispatch remains underneath on the VM stack. Giving
that ErrorEvent the ambient Source origin would incorrectly reject its ordinary
page Error object. Unknown native origins still inspect payload shape ownership
when deciding whether data crosses a registered boundary. Source error reports
remain protected by the earlier job/callback/rejection filters.

Same-world values and unregistered page behavior keep identity. Owned cross-world
plain data becomes a bounded frozen reader-realm snapshot. Getter and onerror
argument use the same event/target cache, so they agree in identity. Unsupported
values fail with a generic TypeError. Construct that error with a synthetic reader
execution context and the original stack saved, so the error cannot capture Source
dispatch/callback frames. Do not inspect the rejected value or run getters/traps.
When argument conversion rejects, an owned handler is not invoked and its guarded
failure is redacted; an ordinary page handler follows native exception reporting
with a reader TypeError, never the raw source payload.

## Evidence

All gates pass in build/error-event-acceptance-test.log: native SDK, sandboxed
helper, ordinary renderer/compositor/process and Firefly/Electron reference.
SDK tests verify source object self-cycles and same-world mutable identity, frozen
page copies, stable snapshots after source mutation, and exact agreement between
property getter/onerror fifth argument, including message/filename/line/column.
Reverse page-to-source arguments use Source prototypes and cached identity while
the page keeps its original object. Source functions never reach the page; hostile
page accessors are rejected by both routes with zero calls. Generic reader error
stacks contain no Source frames. A native page exception raised inside a Source
triggered page callback remains a normal page Error with ordinary onerror delivery.
CustomEvent tests still pass, including explicit constructor provenance, weak-writer
GC, target-cache GC, init/reset and published-copy/fresh-reader revocation behavior.

The actual helper repeats data/getter/argument identity and source-to-page/page-to-
source delivery, function rejection, zero page getter calls, reader stack redaction,
native page reporting from a Source dispatch and published snapshots after detach.
Previous isolation, lifecycle/resource, normal renderer and app controls pass.

## Limits

Cross-world Error instances, functions, promises, host objects, accessors and other
non-data values remain unsupported rather than silently shared. In particular a
Source onerror handler does not receive an arbitrary page Error object yet; the
owned conversion is rejected/redacted. This is not full browser/Electron error
compatibility. Native payload retention can keep an origin realm until the event
collects, as in ADR 0041. Other script-valued fields (MessageEvent.data etc.),
arbitrary borrowed getters/functions, general callback frame/stack and job-origin
isolation, other observers, workers/finalization, navigation/BFCache and hard
CPU/queue/memory containment/accounting remain open. No full audit/reclamation,
new native Windows/Linux/package or visible-desktop support is claimed. Existing
linker/deployment warnings and the locked-desktop visible gate remain.
