# 0031: Guard Promise jobs of owned renderer worlds

Status: internal VM adapter and macOS ARM64 SDK/helper gates implemented. Public
preload/contextBridge/IPC and complete asynchronous revocation remain unsupported.

## Decision

Install WorldJobGuard after the renderer VM's normal hook initialization and
register each owned IsolatedWorld before executing bootstrap code. Track realm
identities weakly; do not retain destroyed worlds through the registry. A callback
or job may not execute for a registered realm whose wrapper world is detached or
whose responsible document is inactive/missing. Inactivity permanently detaches
that world, rather than silently reopening it on a later document activation.

Use two existing VM seams without modifying upstream source. Wrap
host_call_job_callback to resolve the actual callback realm with get_function_realm,
including bound/proxy functions without property access or traps. Delegate live and
unregistered callbacks to the original hook. Revoked proxies use the original
error path and cannot execute their targets.

Also wrap host_enqueue_promise_job. Check owned job realms at enqueue time and
again immediately before execution. Associate the nearest registered realm on the
active execution stack as well, so an owned-world Proxy apply trap targeting a
foreign-realm function cannot evade cancellation through its target's realm.
This conservatively associates jobs scheduled through foreign callees with the
active owned caller. PromiseJob's constructor is private, so an
owned-world job is queued through the public HTML microtask API, with a public
TemporaryExecutionContext enabling script/callback bookkeeping and preserved
ScriptOrModule. Report job errors through LibWeb's exception reporter. All other
jobs with no owned origin retain the original upstream enqueue path; the VM's existing queue-empty
hook still describes the actual HTML queue. Null/unregistered job realms with no owned origin retain
the upstream path, with the callback guard as a second check when applicable.

The callback hook alone is insufficient: the SDK test actually reached detached
DOM wrappers and failed an upstream assertion through async_function_driver's
direct await-resume job, bypassing HostCallJobCallback. The enqueue/execution
guard closes that measured path. Keep await/async-generator tests in update gates.

VM hooks capture reference-counted guard state, not a renderer-session pointer.
The state survives registration-handle/session destruction through VM hook
teardown, retaining only weak realm entries and the original hooks. Shutdown must
not uninstall the guard and reopen already queued callbacks. Explicit endpoint
revocation and world detach still happen in the session before roots are released.

## Evidence and limits

The native SDK gate executes live-world reactions and async/generator jobs, then queues normal, bound and proxy
reactions, thenable invocation, async/await and async-generator continuations.
Include Proxy apply traps targeting a foreign-realm function.
Detach the world, force GC and drain real LibWeb microtasks: none may mutate the
shared document. Resolve a previously pending promise after detach and require
another blocked callback. Page jobs and a different protected live world continue.
An unregistered negative-control world's detached callback still increments a
harmless native counter through the unchanged hook. The guard therefore changes
owned-world execution, rather than relying on default SDK detach behavior.

The actual sandboxed helper test queues the same six categories through a child's
compiled harmless endpoint while a parent JS turn is active. Remove the child
before the next checkpoint; its retained DOM must remain unchanged. Require at
least six blocked jobs logged against the actual renderer PID. The argument is
created in the child's realm, preserving the bridge's foreign-container rejection.
The unchanged-helper ordering control, Promise forwarding and 70-cycle cleanup
tests continue to pass. No user script or privileged binding is enabled.

This is a tested pre-execution Promise-job boundary, not universal source execution
cancellation. Already running async frames and detach during those frames still
need lifecycle integration; no synchronous JavaScript interruption is claimed.
queueMicrotask, timers, DOM listeners/WebIDL callbacks, direct retained function
calls/cross-world callback transfers, finalization behavior, workers, BFCache/cross-process navigation and future
engine job categories still require explicit coverage/integration. Canceled source
promises/async functions may remain unsettled; native target endpoints must be
revoked separately. Dropping a job releases its queue ownership but does not prove
total realm reclamation or bound all queue/CPU/memory usage. Native Windows/Linux,
nondefault COI, packaging and full security audit remain open. The normal Firefly
app still uses the original helper, and Ladybird remains completely unmodified.
