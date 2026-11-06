# 0072: Native invoke/handle request and reply transport

## Decision

Use Ladybird's existing routed native IPC connection, with reserved
`firefly.ipc.invoke.v1` events and `firefly.ipc.reply.v1` commands. Compose the
pinned WebContentPage event entry and WebContent command entry in owned adapters;
keep upstream files unchanged. These internal names do not expose WebDriver or a
JS bridge to the page. The diagnostic helper consumes reserved replies without
installing the production endpoint. Source fingerprints and entry checks fail on
upstream drift; updating Ladybird requires reviewing these seams.

The product preload service owns one endpoint per selected top-level document and
realm. Native invocation captures bounded plain data before sending. The host
resolves the incoming native page/navigable to its canonical active top-level
document, checks the exact window/preload owner, document and realm, and rejects
replayed or over-budget requests. Sender URL comes from canonical native state,
not request arguments. Main handlers still decide application authorization.

## Ownership and scheduling

Renderer endpoints retain Promise capabilities until a bounded reply, deadline or
revocation. Block the native invoke function from direct contextBridge publication;
applications publish reviewed wrapper actions. Document replacement, detach and
shutdown release roots; the source-world job guard continues applying to async
continuations. Settle replies inside an HTML TemporaryExecutionContext and the
owned world frame so the normal microtask checkpoint resumes waiting code.

Host dispatch waits for the Mite event-loop entry boundary. Native reservation
checks and quota error replies do not execute JS in a busy VM. The pure native
BridgeData string constructor supplies error graphs through the normal codec.
Main handler results pass the same bounded data-only codec. Native reply delivery
rechecks document, realm and outstanding request identity. Navigation/crash/window
cleanup frees reservations and queued main timers. Already-started handler side
effects cannot be cancelled or rolled back by navigation.

## Contract and evidence

The [API contract](../api/ipc.md) defines exact limits, data types, errors and the
limited sender-frame snapshot. Actual Electron/Firefly migration tests exercise
initial preload invocation, sync/async/once/missing/error handlers, concurrent
replies, UTF-16/cycles/number edge cases and native sender ownership. Separate
Firefly tests cover getter/proxy rejection, raw-capability publication denial,
16 endpoint slots, old-document replies, destruction and 64 host slots across five
windows with prompt refusal/recovery. Existing wire and world-lifetime gates remain
required. This does not establish a full security audit or complete Electron IPC.

One-way/synchronous messages, ports, subframes, typed-array/full structured-clone
support and native Windows/Linux desktop remain incomplete. The initial IPC checkpoint inlined browser fixtures because local external module
loading failed at the pinned file/CORS boundary. ADR 0073 records the owned loading
adapter and external-module acceptance; IPC fixtures now load their ESM files
directly. The historical workaround is no longer part of the test staging path.
