# Timers and immediates

Status: partial Node-compatible Mite implementation. Globals and `node:timers`
(`timers` alias) expose the same six functions. ESM default/named imports and
CommonJS require share the cached module and its original functions, even if an
application subsequently replaces a global. No Node or LibWeb runtime is involved.

```js
import { setImmediate, clearImmediate } from 'node:timers';

const handle = setImmediate((message) => console.log(message), 'after I/O');
handle.unref();
handle.ref();
// clearImmediate(handle) cancels it before dispatch.
```

## setImmediate(callback, ...args)

Returns an Immediate handle. `callback` must be callable; invalid callbacks throw
TypeError with code `ERR_INVALID_ARG_TYPE` synchronously. Arguments are retained
until invocation/cancellation; `this` is the returned handle. The callback runs
once in libuv's check phase after I/O polling. Calls are FIFO. Calls scheduled by
an immediate callback join the next iteration, rather than extending its current
batch. nextTick and Promise/microtask checkpoints run between individual callbacks.

| Immediate member | Return and lifetime |
| --- | --- |
| `ref()` | Return the handle; a pending call keeps the process alive |
| `unref()` | Return the handle; a pending call no longer keeps the process alive by itself |
| `hasRef()` | Boolean; false once dispatched or cancelled |
| `[Symbol.dispose]()` | Cancel; return undefined |

`clearImmediate(handle)` returns undefined. Repeated cancellation is harmless;
invalid/non-handle inputs do nothing. A fired/cancelled handle cannot be reactivated
with ref. An unreferenced call can still run when other work keeps the loop alive.
Do not use top-level zero-delay timer/immediate ordering as a deterministic API:
process startup and the I/O phase affect it. The tested I/O-completion case runs
its scheduled immediate before its newly scheduled timeout.

Unhandled immediate exceptions and Promise rejections use the
[native process error events](process-errors.md), as do timers and I/O. Without an
exception handler the runtime exits nonzero; handled failures allow later owned
work to continue. The interrupted callback does not resume after its throw.
General async_hooks/async-context semantics remain unimplemented.

## Timeouts and intervals

`setTimeout(callback[, delay, ...args])` and `setInterval` return Timeout handles.
The existing supported subset includes extra arguments, receiver identity,
ref/unref/hasRef, close, disposal and numeric/string cancellation. Delay defaults
to one millisecond; non-finite, below-one or above-int32 delays are clamped to one,
and fractional delays are truncated. Interval callbacks repeat until cleared.

`clearTimeout` and `clearInterval` cancel timer handles/IDs. Immediate handles have
no numeric conversion contract; use clearImmediate for them. Timeout.refresh,
Node's delay/deprecation warnings, Promise interval/scheduler and promisify custom
hooks remain unsupported. [Promise timeout/immediate](abort.md#promise-timer-consumers)
are implemented with signal/ref options. Missing modules/methods reject through the existing
unsupported-builtin or ordinary JavaScript API errors.

## Native ownership and validation

[ImmediateQueue](../../mite/runtime/core/ImmediateQueue.cpp) owns rooted callbacks,
receivers and argument values. A single check handle drains one queue snapshot.
An idle handle forces nonblocking polling only while calls are pending; it is
unreferenced, and the check handle reflects the count of referenced calls. Both
stop when empty and close during teardown before the VM is destroyed.

Actual Node 24.15.0/Mite comparisons cover CJS/ESM FIFO/checkpoints, handle state,
argument/receiver identity, cancellation within a batch, invalid callbacks,
reference liveness, fatal errors, I/O ordering and module/global identity. A
non-GUI embedded-host test forces GC before dispatch and checks that nested calls
require a later native pump iteration. These establish a subset of Node's event
loop, not complete phase/diagnostic/platform parity. Native hosted platform results
are recorded separately in PROGRESS.

References: [Node timers](https://nodejs.org/docs/latest-v24.x/api/timers.html),
[libuv check](https://docs.libuv.org/en/v1.x/check.html) and
[idle](https://docs.libuv.org/en/v1.x/idle.html) semantics.


[util.promisify](util.md) returns the existing timers/promises.setTimeout or
setImmediate implementation for the corresponding callback timer. Value, options,
AbortSignal and ref behavior therefore use those same native handles/subscriptions.
Local Node/Mite tests verify identity, returned values and cancellation.
