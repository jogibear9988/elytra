# node:events

Synchronous EventEmitter dispatch and a cancellable Promise for the next event.
This module runs in standalone Mite and the Firefly main process, without LibWeb.

Status: partial implementation. Emitter behavior, the once signal consumer and
opt-in listener rejection capture pass actual local macOS Node/Mite comparisons.
Fresh Linux/Windows acceptance of these additions remains pending. The behavioral reference
is [Node 24.15.0 events](https://nodejs.org/download/release/v24.15.0/docs/api/events.html).

```js
import { EventEmitter, once } from 'node:events';

const emitter = new EventEmitter();
const controller = new AbortController();
const waiting = once(emitter, 'ready', { signal: controller.signal });
setTimeout(() => emitter.emit('ready', 'connected'), 10);
const [status] = await waiting;
console.log(status);
```

## Import and construction

`events` and `node:events` refer to the same builtin instance. The default export
is EventEmitter; the named EventEmitter export is that same constructor. Legacy
CommonJS consumers can require the same instance. Symbols errorMonitor and
captureRejectionSymbol are exported. The default captureRejections setting is false;
pass `{ captureRejections: true }` to observe rejected listener results.

`new EventEmitter([options])` creates independent listener state. Function-style
initialization with an explicit receiver is supported. Default maximum listeners
are ten; changing EventEmitter.defaultMaxListeners affects emitters without their
own override. Exceeding the maximum schedules a simplified warning; it does not
prevent registering or calling the additional listener.

## EventEmitter methods

| Member | Arguments and result |
| --- | --- |
| on / addListener | `(name, listener)` appends a listener and returns the emitter |
| once | `(name, listener)` removes its wrapper before calling the listener; returns the emitter |
| prependListener / prependOnceListener | Same arguments, inserting before existing listeners |
| emit | `(name, ...args)` synchronously calls the listener snapshot in order; returns whether listeners existed |
| off / removeListener | `(name, listener)` removes the most recently registered matching listener; returns the emitter |
| removeAllListeners | `([name])` removes one event's listeners or all events; returns the emitter |
| listeners / rawListeners | `(name)` returns a new array of originals or actual once wrappers |
| listenerCount | `(name[, listener])` counts listeners, optionally only matching originals/wrappers |
| eventNames | `()` returns registered string/symbol names |
| getMaxListeners / setMaxListeners | `()` reads the effective maximum; `(n)` sets it and returns the emitter |

Listeners receive the emitter as this and the original emitted argument values.
An exception thrown by a listener propagates synchronously from emit. Adding or
removing listeners during dispatch does not change the active emission snapshot.
newListener and removeListener hooks are supported; once wrappers expose their
original callback through listener.

Static listenerCount, getEventListeners, getMaxListeners and setMaxListeners are
available for the supported EventEmitter surface. They do not provide the full
Node EventTarget inspection/limit API.

## Error delivery

Emitting error without an error listener throws. Error instances retain their
identity; other payloads produce ERR_UNHANDLED_ERROR with context. errorMonitor
observers run before ordinary error listeners and do not consume an unhandled
error. By default rejected listener results are not observed. Enable rejection
capture explicitly, or handle asynchronous listener failures in the listener.

## Asynchronous listener rejections

```js
import { EventEmitter } from 'node:events';

const emitter = new EventEmitter({ captureRejections: true });
emitter.on('error', (error) => console.error(error.message));
emitter.on('work', async () => { throw new Error('Work failed'); });
emitter.emit('work');
```

Capture observes returned Promises and thenables. It reads then once, calls a
callable then with its original receiver, and queues rejection delivery through
the existing native process.nextTick queue. Ordinary return values are ignored.
Fulfillment does not emit another event. A throwing then getter or then call emits
error synchronously; synchronous listener exceptions still propagate from emit.

If the emitter has a callable `[captureRejectionSymbol]` method, it receives
`(reason, eventName, ...originalArgs)` with the emitter as this. Otherwise the
same reason is emitted as error; errorMonitor still runs first. Missing error
handlers remain fatal. Capture is disabled while delivering that fallback error
and restored afterward, preventing recursive capture of an async error handler.
A rejected result from that handler remains unhandled; error handlers and custom
rejection hooks must handle their own asynchronous failures.

`EventEmitter.captureRejections = boolean` changes the constructor default for
future instances. Existing instances retain their saved setting. The Node 24
reference inherits that default for falsey constructor values, including an
explicit false; this implementation preserves that measured behavior. Truthy
non-boolean constructor values and non-boolean static assignments throw
ERR_INVALID_ARG_TYPE. Prefer explicit true on the emitter needing capture.

Native forced-GC comparisons verify that pending results retain their receiver
and argument values until rejection delivery, then release them. This is scoped
EventEmitter behavior, not async_hooks context propagation.
Each delivery uses a separate private record, cleared even if its handler throws.
A completed callback still reachable from a conservative native stack scan cannot
therefore retain those values through that record. Repeated calls by a retained
thenable rejection callback continue to receive their original event arguments.

## once(emitter, name[, options])

The module-level once function returns a Promise of the emitted argument array.
It accepts an EventEmitter-compatible object with once and removeListener methods.
Waiting for work also installs a temporary error listener: an earlier error rejects
the Promise with that exact payload. Waiting explicitly for error resolves with
the error event's arguments instead.

Options default to an empty object. Invalid options or options.signal reject the
returned Promise with TypeError/ERR_INVALID_ARG_TYPE. EventTarget sources remain
unsupported; EventTarget itself is available as a separate global.

`{ signal }` accepts a branded Mite AbortSignal from the current realm. A signal
that is already aborted rejects before installing listeners. Later abort rejects
with an Error named AbortError, code ABORT_ERR, and cause equal to the original
signal reason. Completion, rejection, abort and registration failure remove the
owned event/error listeners and cancel the private abort subscription. Independent
waits on the same emitter retain their own listeners.

Aborting the wait does not stop the operation that emits the event. An unresolved
Promise alone does not keep the event loop alive. Waiting promises must be
created before a synchronous emission; use Promise.all when multiple events can
arrive before the first await resumes.

Measured intentional differences from the Node reference apply to this
consumer. A synthetic dispatchEvent(new Event('abort')) does not cancel a wait
while the signal's actual state remains un-aborted. Cancellation during a
newListener registration hook rejects and removes any subsequently installed
listener; the tested Node reference can leave that wait pending. Failure while
registering the temporary error listener also removes the previously installed
event listener; the tested Node reference leaves it registered. These follow
Mite's existing [private cancellation policy](abort.md), independently of public
stopImmediatePropagation. Each difference has an explicit Node/Mite fixture.

## Remaining scope

Async event iterators, EventEmitterAsyncResource/async_hooks,
EventTarget sources for once and complete Node warning/stack/descriptor parity
remain unfinished. General npm or Electron event compatibility is not implied by
these bounded comparisons.
