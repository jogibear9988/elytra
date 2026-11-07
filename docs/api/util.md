# node:util

Status: partial. Shared TextEncoder/TextDecoder classes and promisify are
implemented. The Promise adaptation has local macOS ARM64 Node 24.15 differential,
real filesystem/DNS/child/timer and forced embedded-host GC acceptance. Native
Linux/Windows validation of this new slice remains pending. Other util APIs are
not supplied merely because the module imports successfully.

## promisify(original)

Accept a function using an error-first callback as its final argument and return
a Promise-returning wrapper. Forward the wrapper's receiver and all supplied
arguments, then append the callback. A truthy callback error rejects with that
exact value; otherwise resolve the first success value, or undefined if absent.
Additional success values from an arbitrary callback are discarded. Promise
settlement adopts returned success thenables; repeated callbacks and a throw
following settlement cannot change the first result.

```js
import {promisify} from 'node:util';
import fs from 'node:fs';

const readFile = promisify(fs.readFile);
console.log(await readFile('./package.json', 'utf8'));
```

Synchronous exceptions from the original function become Promise rejections.
The original's normal return value is ignored. Callback invocation may be
synchronous, while Promise continuations still run at a microtask checkpoint.
For an original that uses this, call the wrapper with the right receiver or bind
it explicitly. The wrapper does not bind the original's owner automatically.

Copy the original function's name, length, prototype, own string/symbol property
descriptors and accessors without invoking unrelated property getters. Ordinary
calls to promisify(original) create distinct wrappers. The wrapper receives a
non-enumerable, non-writable, configurable custom marker referring to itself,
so promisify(wrapper) normally returns the wrapper. An original's explicit falsy
custom property is copied over that marker, matching Node's observable behavior.

## promisify.custom

The shared symbol Symbol.for('nodejs.util.promisify.custom') identifies a custom
Promise implementation. A truthy original[symbol] must be a function; return
that exact function and mark it as its own promisified implementation. Inherited
custom properties are recognized. Do not wrap or automatically bind a custom
function. Falsy custom values retain the ordinary callback adaptation path.
Getter exceptions and errors defining a marker are propagated at conversion time.

```js
import {promisify} from 'node:util';

function legacy(value, onSuccess, onError) {
  onSuccess(value);
}
legacy[promisify.custom] = (value) => new Promise((resolve, reject) => {
  legacy(value, resolve, reject);
});
console.log(await promisify(legacy)('ready'));
```

## Builtin-specific results

| Original callback API | Promisified result |
| --- | --- |
| fs.read | {bytesRead, buffer}, preserving the supplied buffer |
| fs.write | {bytesWritten, buffer}, preserving the byte/string input |
| dns.lookup | {address, family}; an array of results with all:true |
| child_process.execFile | {stdout, stderr}; the returned Promise immediately has .child; rejection errors retain .stdout/.stderr |
| timers.setTimeout / global setTimeout | The existing timers/promises.setTimeout function, with value/options/cancellation/ref semantics |
| timers.setImmediate / global setImmediate | The existing timers/promises.setImmediate function |

Filesystem/DNS named results use private realm-owned metadata, preserving ordinary
wrapper creation and leaving the public custom symbol absent. Child processes and
timers use the public custom mechanism. Underlying API options, resource limits,
errors and platform restrictions still apply; promisify does not implement missing
filesystem operations or change the executable's launch semantics.

## Validation and limits

Non-functions passed as original, or truthy non-function custom values, throw
TypeError with ERR_INVALID_ARG_TYPE. Conversion-time metadata/accessor errors are
synchronous; exceptions from an ordinary wrapper's invocation reject its Promise.
Caller-provided custom functions keep their own return/throw behavior.

Node's DEP0174 deprecation warning for originals returning Promises is not emitted.
The wrapper still waits for its callback; functions that only return Promises
should be called directly. No automatic adaptation of non-error-first callbacks,
multiple arbitrary success values, Node internal bindings or new native services
is implied. Callbackify, inspect, format, inherits, types and most other util
members remain absent.

## TextEncoder and TextDecoder

The module exports the same classes installed globally. Existing native encoding,
byte-view/streaming decoder, BOM/fatal and ICU legacy-codec behavior is described
in the [runtime reference](mite.md). Promisify adds no text codec implementation.

See [ADR 0110](../architecture/0110-promisify-callback-results.md) and the
[Node 24.15 util contract](https://nodejs.org/download/release/v24.15.0/docs/api/util.html#utilpromisifyoriginal).
