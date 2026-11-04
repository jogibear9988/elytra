# ADR 0012: FileHandle ownership and pending-operation references

Status: implemented subset, tested against Node v24.15.0 on macOS ARM64.

Implement fs.promises.open with an EventEmitter-derived FileHandle in our JS
layer. It retains a private native GC owner through a traced release function.
The native owner owns only an OS descriptor, never worker-thread JS references.
Explicit close transfers that descriptor to the native close request and disarms
the guard. The guard also closes on finalization/destruction as a fallback. No
Ladybird patches or new engine ABI are required.

Each async handle operation retains its receiver and increments an active count
before dispatch, decrementing it in finally. close emits its event immediately,
matching Node. If operations are pending, retain the descriptor until the active
count reaches zero, then change fd to -1 and submit native close. New operations
can still acquire references while an already-closing handle has pending work,
as observed in Node; once fd is -1 they reject with EBADF and the method syscall.
Repeated close calls share the pending close promise while the descriptor is
still referenced. Calls on an already released handle resolve independently.
[Symbol.asyncDispose] delegates to close. If queue submission throws, close the
transferred descriptor synchronously before rejecting the close promise.

Support read/write (byte-view and string/option overloads), stat/truncate,
sync/datasync, readFile/writeFile/appendFile, close events and async disposal.
Promise whole-file APIs also accept the same FileHandle objects. Keep scalar
numeric-descriptor APIs independent, with their existing caller ownership.
Resource requests still root their completions and never access the JS heap from
workers, as specified in ADR 0011.

Three additional differential workflows compare identity/EventEmitter behavior,
results and buffer identity, byte positions/content, BigInt stats, error shapes,
close-event timing, pending operations plus new operations during close, promise
identity, descriptor reuse after repeated close, and async disposal. The complete
filesystem suite now has 15 shared workflows plus its three explicit policy
fixtures. Error diagnostics are compared by name/code, not full wording/stacks.
Explicit lifetime behavior is tested; forced-GC cleanup has not been validated
and no deterministic collection guarantee is claimed. GC-close deprecation
warnings are not implemented. Applications should always close/dispose explicitly.

Remaining scope includes vector I/O, permissions/times, streams/readLines,
readableWebStream, iterative whole-file writes, object URLs and native platform
validation. FileHandle is not exported as a public constructor. No full Node
FileHandle or filesystem compatibility claim is made. Ladybird remains unmodified.

Reference: [Node FileHandle](https://nodejs.org/docs/latest-v24.x/api/fs.html#class-filehandle).
