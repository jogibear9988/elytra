# 0041: Copy CustomEvent detail across registered world boundaries

Status: internal macOS ARM64 SDK/helper slice tested. Public preload/contextBridge/
IPC and a complete event-field membrane remain unsupported.

## Decision

CustomEvent's generated detail getter returns the native JS::Value directly.
Adapt its single return through the owned callback registry. Two additional owned
compile adapters include generated native/binding includes: the native constructor
and successful init_custom_event detail assignment notify value provenance/reset;
visit_edges traces owned cached copies. Preserve original dispatch guards, type
conversion, data storage, class layout and every byte outside these four seams.
The original checkout/generated binding remain untouched. CMake requires exactly
one native and one generated TU substitution and explicit original-generation
ordering; preservation/drift tests run before LibWeb compiles. This is adapted
engine behavior, not a bit-identical upstream library or a new public SDK.

Within the same world, ordinary detail values keep identity, including arbitrary
functions. Unregistered page-to-page behavior delegates unchanged. Across an owned
boundary, capture with BridgeData's existing bounded data-only graph snapshot and
materialize frozen containers in the getter's realm. Reject functions, proxies,
accessors, host/exotic objects, unsupported primitives and over-budget graphs with
a generic reader-realm TypeError. Never inspect source reasons or execute getters/
proxy traps. Preserve cycles, aliases, holes and data property descriptors.

Track the writing realm weakly at native construction/reinitialization and consult
registered realm activity through the whole guard chain. Shape realm alone is not
provenance: a source object can inherit a foreign prototype. Same-world bypass
requires the owned writer to agree with the reader as well. Revocation stays sticky
when a primitive payload allows its weak writer realm to collect, including events
created before guard registration. Unknown pre-guard object origins also consult
their actual shape realm; raw direct transfer APIs remain outside this guarantee.

Cache up to 64 target-realm copies per event generation for stable repeated getter
identity. The first foreign read takes an immutable snapshot; later source mutations
do not alter it. Reinitialization clears caches only after the original dispatched
check permits the assignment. Previously returned copies remain independent.
Native Event reachability traces cached values; native metadata has weak event/
writer/target keys and is not a root. On world detach, notify every guard layer,
mark its writer records revoked and remove its target caches. Published copies in
other live worlds remain readable; fresh cross-world copies after origin revocation
are denied. Logical/inactive source or target realms deny fresh transfers; current
same-world executing code may still use its existing values.

## Evidence

All gates pass in build/custom-event-acceptance-test.log: SDK, sandboxed helper,
ordinary renderer/compositor/process and native Firefly/Electron reference. Additional
SDK/helper revocation/provenance gates pass in build/custom-event-revoke-test.log
and build/custom-event-provenance-test.log, with the explicit primitive writer GC
check in build/custom-event-origin-gc-test.log. The final provenance run includes
that GC check too.

SDK checks same-world identity, frozen target prototypes, cycles/aliases/sparse
arrays, stable cached identity, snapshot independence, native cache tracing through
GC, successful init invalidation and ignored init during dispatch. Cross-world
functions/proxies/accessors/long strings/symbols reject without traps. A foreign
prototype cannot launder a source object containing a private function. Page values
keep mutable identity; source reads of page accessors reject without executing them.
Published copies survive source revoke, fresh readers reject, target copies collect
on detach and a primitive event's unregistered-then-revoked writer realm collects
while future reads remain denied.

The actual helper dispatches source data twice with an intervening mutation and
requires the same immutable page snapshot. It rejects a raw source function, copies
page input into source prototypes, rejects page getters without running them and
preserves same-world page identity. After source detach the published page copy
remains readable while a new iframe getter realm receives its own TypeError.
All previous helper/SDK/lifecycle, ordinary renderer and Firefly/Electron controls
remain passing. The unregistered helper control uses the same adapted library.

## Compatibility and limits

Frozen cached snapshots are an intentional internal cross-world difference from
raw browser detail identity/mutation sharing. Same-world and unregistered behavior
are unchanged. This does not enable public preload or advertise Electron parity.
The original native detail still retains its original payload until reinitialized
or the event collects; an object payload can therefore retain its source realm
after source detach. This is capability isolation and target-cache release, not
complete source realm reclamation. Native metadata/snapshot scratch accounting,
aggregate memory/CPU/queue bounds, arbitrary borrowed getter/direct function
transfers, other script-valued fields (ErrorEvent.error, MessageEvent data, etc.),
other observers, workers/finalization and navigation/BFCache remain open. No full
security audit, new native Windows/Linux/package or visible support is claimed.
Ladybird remains unmodified and no newer pin was tried. Existing deployment/linker
warnings and the separate locked-desktop visible gate remain.
