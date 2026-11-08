# ADR 0138: Bounded document callbacks through contextBridge

Status: accepted for the locally tested macOS native/production-helper subset.
The later [ADR 0139](0139-private-ipc-module-redaction.md) supersedes the raw-module/event
refusal below with exact native module redaction; other stated limits remain.

## Context

Code-OSS publishes preload functions that accept document listeners. Data-only
function arguments prevent subscriptions even when main-to-preload IPC works.
A callback must not inherit privileged frames, keep dead documents alive through
a permanent cache, or bypass lifecycle/pending-work limits during API reentry.

## Decision

Share a callback registry across each document's ContextBridge function exports.
Capture ordinary exact-principal-realm function arguments as frozen source-world
NativeFunction proxies, retaining graph aliases and stable live identity across
exports. Cache original/proxy/owner weakly. A proxy owns its endpoint and original
function; pending Promise reactions can retain the endpoint after proxy collection.
Keep such endpoints weakly registered until they die so detach still revokes them.
Never run JS in a native closure destructor triggered by GC.

Copy/freeze supported data in each direction. Strip the complete execution context
stack for calls back into the principal realm. Supply a fresh principal execution
context with HTML callback settings so Promise observation has a principal incumbent,
without privileged stack frames, a source job cancellation generation or an early
microtask checkpoint before the outer call returns. Guard
caller provenance and allow bounded reentry only while a genuine principal callback
is running. Use a fresh isolated execution context for that reentry; never reuse the
outer saved base frame. Keep ordinary recursive world evaluation refused.

Observe actual same-realm native Promises directly, bypassing then/constructor/
species hooks. Sanitize exceptions and refuse polluted resolution prototypes.
Limit callback endpoints and aggregate pending callback Promises to 64 each,
arguments to 64, and nested entries to 32. Roll back only new endpoints if capture
fails; commit at delivery because application code can retain arguments then throw.
Revoke on document/world teardown, clear roots and reject pending bridges. Principal
continuations remain independent and late results cannot revive source endpoints.
No changes to Ladybird or Mite's dependency boundary are required.

## Acceptance and limits

The real native LibJS/LibWeb isolation gate covers cycles/aliases, receiver/realm/
constructor boundaries, clean stacks, stable on/off identity, weak collection,
capture rollback without getter/proxy execution, repeated exact endpoint budgets,
64 pending callbacks and recovery, bounded nested API calls, Promise hook bypass and outer-turn ordering,
error sanitation, dead-proxy pending ownership and detach with surviving principal
continuations. Existing isolation/job/DOM/Promise gates remain green.

One hidden production-helper IPC batch exercises actual document callbacks,
unsubscription across exported methods, API reentry, asynchronous results and fresh
subscriptions after navigation. A separate actual Electron run checks the common
positive callback contract. Electron does not preserve proxy identity across the
separate methods in this test and supplies mutable callback input; those Firefly
assertions are deliberately excluded from the reference.

Function-valued callback arguments/results, bound/proxy functions, binary/native
objects and ports remain refused. Raw IPC event forwarding also fails because its
sender contains the private module; applications must discard the event and send
plain payloads. This does not yet run Code-OSS's unchanged preload or workbench.
Fresh Linux and native Windows acceptance remain open. Service worker/cache and
protocol startup requirements remain higher-level blockers, not inert flags to skip.
