# node:events

Synchronous EventEmitter dispatch and a cancellable Promise for the next event.
This module runs in standalone Mite and the Firefly main process, without LibWeb.

Status: partial implementation. Existing emitter behavior and the new once signal
consumer pass actual local macOS Node/Mite comparisons. Fresh Linux/Windows
acceptance of this cancellation addition remains pending. The behavioral reference
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
captureRejectionSymbol are exported, but captureRejections:true remains explicitly
unsupported. The default captureRejections setting is false.

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
error. Promise-returning listeners are not automatically observed: captureRejections
remains unavailable, so callers must handle asynchronous listener failures.

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

Async event iterators, EventEmitterAsyncResource/async_hooks, captureRejections,
EventTarget sources for once and complete Node warning/stack/descriptor parity
remain unfinished. General npm or Electron event compatibility is not implied by
these bounded comparisons.
