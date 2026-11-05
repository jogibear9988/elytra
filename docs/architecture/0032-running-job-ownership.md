# 0032: Retain world ownership across running jobs

Status: internal adapter and macOS ARM64 SDK/helper gates implemented. Public
preload/contextBridge/IPC and universal asynchronous revocation remain unsupported.

## Decision

Extend IsolatedWorld's rooted execution boundary to guarded asynchronous frames.
The same boundary retains the owner, pushes its owned realm context, rejects
ordinary recursive evaluate/call entry and defers physical detach until context
pop. Detach requests revoke the owner logically immediately: realm access and
further jobs/capabilities must be denied while the running JS turn finishes.
This does not interrupt synchronous JavaScript already executing in that turn.

WorldJobGuard now stores weak realm-to-native-owner associations. On execution,
acquire strong references for the owned origin and actual job/callback realms.
Enter their execution boundaries unless already protected by an outer boundary;
normal nested JS Promise callbacks reuse the existing frame, rather than being
mistaken for recursive host evaluation. Missing owners and logical detach requests
are cancellation conditions, including while SDK wrappers remain physically live.

Inactivity requests owner.detach, rather than bypassing the owner through the raw
WrapperWorld API. This keeps wrappers/context alive until a currently running
frame unwinds. Direct SDK detach is still recognized as invalid, but native
integration code must use the owner for deferred teardown. The weak registry does
not keep owners alive between jobs. Last external release during a job is safe;
the job holds ownership until it returns, then native destruction can finish.

Keep approved target Promise settlement independent from source generation
cancellation. Execute built-in resolve/reject in a saved-stack synthetic target
context, with the copied value/error rooted. Otherwise the job guard's conservative
source-origin association could cancel the live parent's rejection handler during
source-frame removal, or cancel a continuation for data already published before
later source revocation. Source frames are restored immediately; only the native
settlement call runs in this scope. Error construction still excludes source stacks.

## Evidence

Native tests run a reaction, await continuation and async-generator continuation.
Inside each job, a native probe requests detach and drops the last application
owner reference. Require logical revocation, physical wrappers still live during
the callback, successful subsequent DOM access in the current turn and rejection
of a newly scheduled job. After checkpoint, require expired native weak ownership,
SDK physical detach and restored execution-context depth, including GC before
dispatch. Existing pre-start cancellation/live/foreign-realm controls still pass.

A second SDK regression forwards an asynchronous source result, detaches its
producer between publication and target handler execution, then requires the
already approved frozen result to reach the target handler.

The actual sandboxed helper adds only compiled harmless probes. A reaction, await
continuation and async-generator continuation each remove their own iframe. Their
current DOM access completes, the next source job never runs, and a live parent
receives the sanitized rejection for the now revoked endpoint. The negative
ordering control, Promise forwarding and 70 frame-cycle gates still pass. No user
preload, privileged binding or normal Firefly app helper selection changed.

## Remaining boundaries

This completes ownership for the guarded Promise job/callback paths, not every
LibWeb callback. queueMicrotask, timers, DOM/WebIDL listeners, direct/cross-world
function transfers, finalization/worker paths, BFCache and cross-process navigation
still need integration and adversarial coverage. Source error/rejection-event
routing must be checked for isolation before enabling privileges. Already running
JS can finish its current turn; all future privileged endpoints must enforce
logical document/world revocation on each entry. No total realm collection, hard
CPU/memory bound, new native platform support, package or full sandbox audit is
claimed. Ladybird source stays unmodified.
