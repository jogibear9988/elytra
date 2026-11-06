# 0074: Native check-phase immediate queue

Use a separate ImmediateQueue in the shared Mite runtime, rather than disguising
setImmediate as a zero-delay timer or Promise job. This preserves the required
post-I/O check phase and gives standalone and externally pumped hosts the same
implementation without a dependency on Firefly, LibWeb, Node or V8.

One libuv check handle owns the phase; a temporary unreferenced idle handle forces
a zero-timeout poll while calls await dispatch. A referenced-call counter controls
the check handle's liveness. Empty queues stop both handles, avoiding idle spinning.
The scheduler snapshots its FIFO for each check callback. New calls, including
ones made by nextTick/microtasks during a callback checkpoint, await the next
iteration. Cancellation removes rooted records immediately; stale native IDs are
periodically compacted so repeated cancellation before yielding stays bounded.

Retain callback, receiver and argument roots through the individual callback and
its EventLoop::dispatch checkpoint. Remove the record before calling JS, making a
fired handle unreferenced and non-reactivatable. Fatal errors stop the rest of the
batch. Teardown releases roots, stops and closes handles; close callbacks free
native storage only while EventLoop still owns the uv loop. No queued JS runs in
that shutdown drain.

Bootstrap is maintained ESM, compiled through the existing reproducible synchronous
adapter. It captures/removes private native hooks. Immediate IDs live in a WeakMap,
not a public cancellation channel. node:timers captures original globals before
application code and shares those objects across ESM/CJS. Timer promises, async
hooks/context and full Node phase parity remain separate work.

The [API contract](../api/timers.md) and differential suite define supported
behavior. The existing non-GUI embedding probe additionally counts actual pump
iterations and forces collection while callbacks/arguments are pending. Desktop
build compatibility is checked by compiling the same runtime into Firefly; GUI
acceptance is delegated to hosted CI for this change.
