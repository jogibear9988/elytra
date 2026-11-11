# Async context

Mite's `node:async_hooks` provides native-backed `AsyncLocalStorage` and
`AsyncResource`. Promise reactions capture context when registered; the direct
already-settled `await` path captures it when its resume job is enqueued. Timers,
next ticks, microtasks, immediates, worker completions, TCP/local sockets, DNS,
filesystem watchers and child-process callbacks carry traced resource contexts.
TLS/HTTP parsing runs on the underlying socket context. Resources restore the
previous context after execution, including throws. Storage frames retain real
JavaScript values through GC edges; shutdown revokes native facade ownership.

`run`, `enterWith`, `exit`, `getStore`, `disable`, static `bind`/`snapshot`, and
`AsyncResource.runInAsyncScope`/`bind` are implemented. As in Node24's default
frame implementation, `disable` removes this storage from the current shared
frame; a snapshot of a different frame retains its value. `run` restores its own
store and preserves changes to other storage instances. A frame admits at most
256 distinct storage instances, including bindings reserved for enclosing `run`
restoration. New admissions fail before consuming that reserved capacity. Resource IDs are Mite's actual monotonic safe
integer identities, not Node/V8 identity parity.

Constructor options, `createHook`, and `executionAsyncResource` explicitly fail.
`emitDestroy` returns the resource; no hook observers are admitted, and execution
after this call remains allowed as in Node. Native platform observers and external
embedding callbacks outside the resource paths listed above have not been
validated for context propagation. This does not establish full `async_hooks`
compatibility or AgentHost workflow acceptance. Unhandled-rejection and late-handled
notification contexts have not been validated; their process notifications may run
in the checkpoint context. Uncaught exceptions from timers, immediates, next ticks
and microtasks are reported within the failing resource context.

The seven focused actual Node/Mite propagation groups pass on the current macOS
native build. Dedicated native regressions also pass uncaught timer/immediate/tick/microtask
error-handler context and restoration-slot exhaustion. Fresh other-platform execution remains unaccepted.
