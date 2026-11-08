# ADR 0139: Drop private IPC modules at the contextBridge boundary

Status: accepted for the locally tested macOS native/production-helper subset.

## Context

The pinned Code-OSS preload passes document listeners directly to ipcRenderer.on
and once. Its IPC event includes the private module as event.sender. Data-only
callback snapshots refuse that graph, blocking subscriptions even though callback
forwarding now works. Publishing the native methods would give a document an
unrestricted IPC capability, violating the narrow preload boundary.

[Electron documents an empty object for a whole ipcRenderer module crossing the
bridge](https://www.electronjs.org/docs/latest/api/context-bridge#exposing-ipcrenderer).
The actual Electron 45.2.3 reference instead copies own EventEmitter bookkeeping,
with ordinary prototypes and no IPC methods. Firefly adopts the documented empty
object contract and its existing frozen destination-data policy. Do not advertise
exact object shape/freeze parity with that reference.

## Decision

Register the exact native ipcRenderer exports object with its owning isolated
world. Keep weak GC identities, not string tags, prototype tests or attacker
properties. Clear entries on detach. Only source-owned ordinary engine objects
can be registered; registration has no public JavaScript entry point.

Add an explicit native object-redaction policy to graph capture. After checking
exact VM/realm/ordinary engine class, replace a designated module with an empty
ordinary object node, charge the normal graph value budget, retain aliases, and
skip all its properties. No getter, proxy or expando code runs. Other objects,
functions and existing graph budgets keep their prior validation.

Use one context snapshot helper to apply this policy consistently to API
publication, synchronous/Promise function results and source-to-document callback
arguments. Do not apply it to principal-to-source values or raw IPC/wire capture.
Keep individual native IPC methods on the existing export-denial list. Passing
only a method, a proxy around the module or a copy containing native methods still
fails. The destination receives no original module pointer, source prototype,
private listener data or native function.

## Acceptance and remaining work

The actual-engine isolation gate covers direct/nested API publication, shared
aliases, sync/Promise results and callback events. It asserts destination-owned
frozen empty ordinary objects, source-method/spoof/getter refusal without traps,
strict default capture, foreign-realm refusal, weak metadata collection and detach.
A noinline allocation frame separates conservative stack roots from the weak
metadata lifetime test. Existing isolation, callback, Promise, jobs and DOM gates
remain green. Ladybird is unchanged; there is no wire version change.

The existing single hidden production-helper IPC batch now directly registers a
document callback with native once, receives event.sender without capabilities,
and verifies module publication plus sync/Promise results over fresh documents.
The real Electron common reference checks absence of IPC methods; exact emptiness
and freezing are separate Firefly assertions because of the observed difference.
The final local macOS Release build and both public reference/Firefly gates pass.
No broad desktop suite is used. Fresh Linux/native Windows execution remains open.

This does not publish the full unmodified Code-OSS preload: its source accessors,
process/options, webFrame/webUtils and port handoff still need implementation and
acceptance. Binary values/MessagePorts and the actual service-worker/cache/protocol
startup blocker remain priority work. Narrow payload wrappers remain recommended;
no arbitrary native object, function-valued result or full DOM Event membrane is
introduced by this module-specific rule.
