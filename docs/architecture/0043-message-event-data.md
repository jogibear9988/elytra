# 0043: Protect MessageEvent data after native message deserialization

Status: internal macOS ARM64 SDK/helper slice tested. Public preload/contextBridge/
IPC, transferable capability policy and full message compatibility remain unsupported.

## Decision

Window::deliver_posted_message already structured-deserializes in the receiver's
principal realm. MessageEvent::data then returns its raw stored JS::Value to every
wrapper world. Keep serialization, task queueing, origin checks and delivery upstream;
route only the generated data getter through the existing EventValues reader.
Add native cache tracing and successful initMessageEvent invalidation through an
owned native TU adapter. Generated JS constructor records its writer realm explicitly.
Native-origin events are recognized lazily on first read and do not inherit an
ambient Source writer. Preserve class layout and every other source byte.

CMake requires one native and one generated MessageEvent source substitution,
retains original bindgen dependencies and runs source preservation/drift tests.
There are no vendor edits or committed upstream algorithm copies. As with earlier
adapters, the binary is adapted and candidate pin upgrades need semantic review.

Reuse the bounded frozen graph snapshots and per-event/reader identity cache from
ADRs 0041–0042. Same-world and unregistered page behavior remains unchanged.
Cross-world functions/proxies/accessors/unsupported values reject generically,
without traps or Source frames in reader errors. Preserve cyclic/aliased data,
sparse arrays, target prototypes and revocation/target-cache cleanup semantics.
Successful legacy init changes data generation; init during dispatch remains a
native no-op. Leave origin/lastEventId/source and ports algorithms untouched.
Existing upstream world-aware ports cache and WindowProxy rewrapping remain in use.

A postMessage transfer is already a serialized publication, not a retained Source
reference. Its principal receiver gets the original mutable deserialized page data;
a registered Source listener gets its own frozen snapshot. A queued approved post
can still reach a live page after Source detach, while the Source listener's native
registration is removed. Do not mistake that delivered post for a new Source transfer.

## Evidence

All gates pass in build/message-event-acceptance-test.log: SDK, sandboxed helper,
ordinary renderer/compositor/process and native Firefly/Electron reference. SDK
checks same-world identity, cyclic/sparse frozen cross-world copies, stable cache,
GC tracing, snapshot independence, successful init reset and ignored init during
dispatch. Origin/lastEventId, WindowProxy source identity, empty frozen ports and
ports-cache reset remain correct. Functions/proxies/accessors/large strings/symbols
cannot cross; trap counts stay zero. Page-owned values stay mutable, reverse reads
copy into Source, published snapshots survive origin detach, fresh readers reject
and detached target copies collect.

The actual helper dispatches a source MessageEvent twice around mutation, requires
a single frozen page snapshot, rejects a raw function and rejects a hostile page
getter without calling it. Source sees its own original data. Page-to-Source data
uses Source prototypes. Native postMessage passes in both directions with a Source
reply, expected page identity/mutability and trusted native event flags. Native
serialization rejects a function with DataCloneError. A source posts then detaches:
the page receives the serialized value, the Source listener stays silent and the
previously published page snapshot remains available. A simple WindowProxy source
is rewrapped into the page: its constructor cannot access a Source global secret.
All previous field, callback, resource, lifecycle and ordinary app gates pass.

## Limits

This is data-field isolation, not IPC or sender authentication. Synthetic origin
strings remain caller-supplied; a test label or trusted event flag is not a main-
process authorization protocol. Non-data structured-clone types (Map, ArrayBuffer,
typed arrays, transferred host objects, etc.) remain unsupported for owned snapshot
reads. Actual transferred ports, workers/BroadcastChannel, source proxy edge cases,
other messaging APIs and full cross-world clone parity remain unverified. Only the
basic WindowProxy/empty-port controls above are claimed. Native raw payload/source
retention may retain origin state until event collection. Other script-valued
fields, general callback frame/job-origin isolation, other observers, custom element/
shared registry boundaries, arbitrary transfers, navigation/BFCache and hard resource
containment/accounting still need work before public preload. No full audit or new
Windows/Linux/package/visible-desktop support is claimed. Ladybird stays unmodified;
no newer pin was tried. Existing deployment/linker warnings and the separate locked-
desktop visible gate remain.
