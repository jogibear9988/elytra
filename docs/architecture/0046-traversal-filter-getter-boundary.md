# 0046: Keep traversal filter getters inside their owning world

Status: internal macOS ARM64 SDK/helper slice. Public preload/contextBridge/IPC and
the remaining callback/API membranes are incomplete.

## Decision

Native NodeIterator and TreeWalker filter getters return their stored callback
object. Invocation guards from ADR 0045 cannot protect a raw callback after that
reference reaches another world and is called directly. Route the two generated
filter getter returns through an owned policy before returning to JavaScript.

Substitute the two generated binding TUs in our LibWeb build, preserving bindgen
ordering and changing only each filter_getter return statement. Extend the existing
native NodeFilter adapter with a constructor notification. Exact source counts,
generation dependencies, getter/constructor seam tests and byte-for-byte reverse
preservation guard candidate upgrades. Vendor/generated originals, layouts,
algorithms and license notices stay untouched.

WorldJobGuard keeps weak native-filter/writer-realm metadata. Construction records
the active native entry realm, independently of callback object/prototype shape or
CallbackType's principal settings object. Notifications propagate through nested
guard layers; reads can consult an earlier layer when only it knows the origin.
World revocation marks provenance sticky before weak writer expiry. Dead filter
rows are pruned on creation/read; metadata does not trace callback values or realms.

Without registered realms, known ordinary page filters retain their original value.
When a registered realm participates, same-world bypass requires agreement among
reader, constructor writer, associated callback realm and actual callable realm.
Otherwise throw a fixed reader-realm TypeError from a synthetic context without
Source frames. Object getters/proxy traps are never called by the read policy.
Null filters stay null for active readers. Exiting/revoked readers reject. Unknown
provenance with no earlier guard to validate it and unresolvable callable realms
also reject explicitly; there is no speculative raw-reference fallback.

This intentionally rejects cross-world callback access rather than exporting a
live function/object proxy with unimplemented semantics. Native traversal can still
invoke a live foreign filter through ADR 0045, without publishing its raw callback.
A Source-created native filter containing an ordinary page callback also needs
writer agreement; the page's possession of its original callback does not authorize
a read through the Source-created native container.

## Validation

All four acceptance gates pass in build/filter-access-acceptance-test.log; SDK/helper
follow-up coverage passes in build/filter-access-final-test.log. A deliberately
unobserved constructor and a revoked callable proxy both reject generically.

SDK tests retain same-world filter identity, reject page-to-Source, Source-to-page
and Source-to-another-world reads, and keep native invocation working. Source
getter failures use the reader's TypeError. Foreign-prototype callbacks cannot be
published through the getter and their acceptNode accessor stays untouched. A
Source-created filter with a foreign page callback remains denied after its weak
writer realm collects. A TreeWalker/filter without other references collects,
showing the metadata is not an extra native root. A noinline fixture factory avoids
stale Source wrapper pointers in the conservative GC stack during those assertions.

The actual sandboxed helper publishes only fixed native traversal wrappers through
a compiled fixture hook. It tests NodeIterator and TreeWalker getters for functions,
bound functions, callable proxies, callback objects and object proxies. Source own
identity remains intact; page getters throw without running traps or exposing
Source stacks, before and after Source detach. Principal callback identity and null
filter controls remain live. The hook has no main-process privileges and is absent
from the ordinary production helper. Existing native traversal/lifetime and ordinary
renderer/Firefly regressions remain required.

## Limits

This closes these two generated getter paths, not all native object/callback
transfers. Borrowed getters/direct functions, generic WebIDL modes, shared registries,
other callback resources, workers/finalization, navigation/BFCache and aggregate
resource bounds remain open. Unknown provenance or unresolved callable realms can
reject otherwise ordinary callback access while a guard is installed; no full DOM/
Electron parity is claimed. Native containers can retain original callbacks/realms.
In this pin Document traces its registered NodeIterators; dropping a JS iterator
reference does not prove its native filter was reclaimed. The weak-metadata GC test
therefore uses TreeWalker. No complete realm/native reclamation or security audit.
Public preload/contextBridge/IPC stay unsupported; standalone Mite and Ladybird's
pin/source are unchanged. No new platform/package or visible-desktop validation;
existing deployment/linker warnings remain.
