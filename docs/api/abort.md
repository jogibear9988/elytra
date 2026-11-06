# AbortController, AbortSignal and events

Status: partial Mite cancellation infrastructure, validated against Node 24.15.0.
The main process/CLI now has Event, EventTarget, DOMException, AbortController and
AbortSignal globals without LibWeb or Node. Browser renderers continue using
Ladybird's own DOM implementations. These are separate object worlds.

## Controllers and reasons

```js
import { setTimeout as sleep } from 'node:timers/promises';
const controller = new AbortController();
const waiting = sleep(10000, 'done', { signal: controller.signal });
controller.abort(new Error('Stop requested'));
try {
  await waiting;
} catch (error) {
  console.log(error.name, error.code, error.cause.message);
}
```

`new AbortController()` creates one stable signal. `abort([reason])` synchronously
marks it aborted and dispatches abort notification once. The first reason wins;
subsequent calls do not change it. Explicit reasons retain their value/identity.
An omitted or undefined reason creates a DOMException named AbortError (code 20).

| Signal member | Contract |
| --- | --- |
| `aborted` | Initially false, then permanently true |
| `reason` | Initially undefined, then the original reason |
| `throwIfAborted()` | Throw that exact reason if aborted, otherwise return undefined |
| `onabort` | Nullable event-handler attribute |
| `addEventListener` / `removeEventListener` | Portable EventTarget listener interface; once/object listeners supported in the tested subset |
| `AbortSignal.abort([reason])` | New already-aborted signal |
| `AbortSignal.timeout(delay)` | Abort with TimeoutError DOMException (code 23) after the delay; the deadline alone does not keep the process alive |
| `AbortSignal.any(signals)` | Compose an iterable of signals; first existing/actual abort supplies the reason, duplicates do not emit twice |

AbortSignal cannot be directly constructed. Wrong controller/signal receivers
throw TypeError; error codes are tested for the supported cases, not stack/text
parity. timeout requires a numeric uint32 delay, rejecting non-integral/out-of-range
values with ERR_OUT_OF_RANGE and non-numbers with ERR_INVALID_ARG_TYPE. It uses
the existing native timeout policy: delays above signed-int32 clamp to one
millisecond without Node's overflow warning. any validates the complete iterable
before returning, including invalid entries after already-aborted signals.

Composition updates dependent aborted/reason state before source listeners run.
Private consumer abort algorithms run independently of stopImmediatePropagation.
A public synthetic abort Event does not change signal state or run those private
algorithms. Overriding dispatchEvent remains observable as in the Node reference.

## Event and exception subset

EventTarget and Event use pinned event-target-shim 6.0.2 with complete license,
source and lock provenance. Basic dispatch identity, duplicate/removal, once and
object listeners, cancellation and preventDefault are tested. Listener exceptions
are scheduled through nextTick as fatal errors after dispatch, matching the tested
Node order. Node-specific listener warnings, private extensions, all option/receiver
combinations, EventTarget signal-option edge cases and full DOM conformance remain
unverified. EventEmitter APIs are separate.

DOMException extends Error and provides branded name/message/code getters, legacy
numeric constants and the DOMException string tag. Error text, stacks and complete
property-descriptor parity are not claimed.

## Promise timer consumers

`node:timers/promises` and `timers/promises` share the same default/named functions:

- `setTimeout([delay = 1[, value[, options]]])` returns a Promise of value.
- `setImmediate([value[, options]])` returns a Promise of value in the check phase.

Options are `{ signal, ref = true }`. Signals must be from this realm's Mite
implementation; foreign/polyfill signals are rejected explicitly. Aborted-before-start or later abort rejects
with an Error named AbortError, code ABORT_ERR and cause equal to the signal's
reason. Later abort cancels the actual native handle. Settled requests release
subscriptions. Invalid options, ref types, signals or timeout delay types reject
the returned Promise; they do not synchronously throw. ref:false leaves native
work unreferenced, so an unresolved Promise alone does not keep Mite alive.

Promise setInterval and scheduler remain unavailable. Installing these globals
does not implement AbortSignal support in fs, child_process, TCP, HTTP or stream
operations that explicitly rejected that option previously. TLS/HTTPS/fetch remain
separate work. Check each service contract before using signal there.

## Explicit alpha differences and remaining acceptance

Two actual Node/Mite differences have dedicated assertions:

- Portable abort events have isTrusted:false; Node emits true. This is event
  metadata, not an authority boundary for application authorization.
- Manually dispatching a synthetic abort event cancels Node 24's Promise timer
  while signal.aborted is still false. Mite's private consumer algorithm waits for
  actual state change; the timer instead completes normally.

Listener-only composite signals are retained by reachable source signals while
an abort listener, onabort handler or private consumer needs notification. Removing
listeners, consuming once listeners or clearing onabort releases that retention.
Unreachable source/composite cycles remain eligible for collection. Timeout signals
with observers survive to their deadline; unused collected timeouts release their
native timer handles through asynchronous finalization.

Forced-GC tests compare these cases, guarded listener removal and weak unregister
tokens against Node. They also check actual native timer counts in Mite. Finalizers
are nondeterministic: application correctness must use explicit cancellation and
resource closure rather than assume prompt collection. Complete Web event types,
arbitrary dependency graphs and all collection schedules remain unverified. Native
platform gates and broader cancellation integration remain required.

See [timers](timers.md), [async lifetime](../concepts/async-and-lifetime.md) and
[Node cancellation globals](https://nodejs.org/api/globals.html#class-abortcontroller).
