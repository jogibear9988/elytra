# 0036: Isolate MutationObserver delivery and release registrations

Status: internal macOS ARM64 SDK/helper implementation tested. Public preload,
contextBridge and IPC remain unsupported.

## Decision

Keep Ladybird unchanged. A third owned LibWeb translation-unit adapter includes
its complete DOM/MutationObserver.cpp implementation after preincluding the
original class declaration. Adapt only m_node_list expressions to a reference
view: append observes successful registrations, clear observes disconnect, and
iteration forwards to the original weak node list. Do not copy the list, modify
class layout or replace observation/notification/disconnect algorithms. CMake
requires exactly one substitution for each of the three adapted translation
units; the new token guard expects three m_node_list occurrences.

WorldJobGuard associates observers with registered callback realms using weak
realm/observer metadata. Repeated registration on multiple nodes does not duplicate
ownership. Explicit disconnect removes metadata; re-observe restores it. Physical
world detach first removes metadata and roots the affected native observers, then
calls their original disconnect method. This removes ordinary/transient node
registrations and queued records without disconnecting page observers. Logical
detach during a guarded frame defers physical release until that frame returns.
The guard blocks late Report-style delivery independently of resource release.

The pinned engine builds callback arguments in the callback settings realm,
which is the document's principal realm for these isolated worlds. Passing those
wrappers unchanged would expose page prototypes to source code. For recognized
native MutationObserver delivery, the owned callback gate reconstructs the dense
record Array and wraps each native MutationRecord and the observer/receiver in
the associated isolated wrapper world. Read only native array own descriptors;
never run foreign getters or copy page expandos. Preserve observer identity,
receiver, record order and the shared native DOM. Other callbacks delegate to the
existing paths; this does not establish a general callback argument membrane.

## Evidence

SDK tests collect garbage before taking native snapshots, deliver a mutation with
poisoned page Array/MutationObserver/MutationRecord prototypes, and require source
prototypes, observer/receiver identity and source DOM wrappers. Repeated observe,
multiple nodes, explicit disconnect and re-observe verify ownership accounting.
Detach with a pending record requires empty native node lists and record queues,
no further source callback, and continued page observer delivery.

The sandboxed helper repeats prototype poisoning and source-only detach while
keeping the document/page observer alive. It requires a stable source DOM result,
continued page delivery, no page getter execution and a native cancellation count
from the matching renderer PID. SDK/helper, ordinary renderer/compositor/process
and Firefly/Electron reference gates pass. The upstream helper control shares the
adapted library but registers no owned worlds; it is not a bit-identical engine.

## Limits

Other observer types, event listeners, other WebIDL conversions/results, direct
cross-world function transfers, workers/finalization, navigation/BFCache and hard
resource budgets remain incomplete. A queued upstream notification may retain an
observer until its checkpoint; this does not prove immediate realm reclamation.
No public preload, new platform/package support or full audit is claimed. Existing
linker/deployment warnings and the separate locked-desktop visible gate remain.
No newer Ladybird pin was evaluated.
