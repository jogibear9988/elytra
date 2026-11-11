# node:util

Status: partial. Shared TextEncoder/TextDecoder classes, promisify and inherits are
implemented. inherits passes local Node 24.15/Mite differential acceptance; fresh
Linux/Windows execution for that addition is unverified. The Promise adaptation has local macOS ARM64 Node 24.15 differential,
real filesystem/DNS/child/timer and forced embedded-host GC acceptance. Native
Linux/Windows validation passes at a56d183 in
[Linux run 42391944791](https://github.com/jogibear9988/elytra/actions/runs/42391944791)
and [Windows run 42391944810](https://github.com/jogibear9988/elytra/actions/runs/42391944810). Other util APIs are
not supplied merely because the module imports successfully.

## inherits(ctor, superCtor)

The legacy constructor helper installs the parent's instance prototype above the
existing child prototype. Existing child methods and its constructor property
survive. A writable/configurable, non-enumerable ctor.super_ stores superCtor.
The constructor's static prototype is unchanged; static properties are not copied.
Use modern class extends for new code; this API serves real CommonJS dependencies.

```js
import { inherits } from 'node:util';
import { EventEmitter } from 'node:events';

function LegacyEmitter() {
  EventEmitter.call(this);
}
inherits(LegacyEmitter, EventEmitter);
const emitter = new LegacyEmitter();
emitter.on('ready', () => console.log('ready'));
emitter.emit('ready');
```

Like the Node reference, this deliberately permits object-like constructor records
where ordinary property/prototype operations succeed. Missing/null constructors
or an undefined parent prototype throw ERR_INVALID_ARG_TYPE. Other invalid targets,
nonextensible prototypes and descriptor failures propagate ordinary TypeErrors;
getter exceptions propagate unchanged. A failed prototype mutation can leave the
already installed super_ property. This helper is not an atomic class-construction
operation and does not validate arbitrary objects as callable constructors.

The local differential gate compares module identity, instance/EventEmitter behavior,
descriptors, static-parent preservation, getter propagation and observable failure
side effects. It does not establish whole-package compatibility. The actual Code-OSS
ZIP consumer now also passes with genuine yazl, source-analyzed CommonJS named
exports and independent archive CRC/extracted-byte verification. The inherits
tests alone do not establish that package acceptance.

```sh
python3 tooling/tests/test_inherits.py --mite build/release-mite/bin/mite
```

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
| child_process.exec / execFile | {stdout, stderr}; the returned Promise immediately has .child; rejection errors retain .stdout/.stderr |
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
is implied. Callbackify and other unlisted util members remain absent. The implemented
inspection/formatting and deprecation APIs are described below; inheritance and
native type predicates have their own documented subsets.

## TextEncoder and TextDecoder

The module exports the same classes installed globally. Existing native encoding,
byte-view/streaming decoder, BOM/fatal and ICU legacy-codec behavior is described
in the [runtime reference](mite.md). Promisify adds no text codec implementation.

See [ADR 0110](../architecture/0110-promisify-callback-results.md) and the
[Node 24.15 util contract](https://nodejs.org/download/release/v24.15.0/docs/api/util.html#utilpromisifyoriginal).

## Bounded inspection and formatting

`inspect`, `format` and `formatWithOptions` now format real primitives, ordinary
own-data objects, arrays, Map/Set, dates, regular expressions and cycles.
`inspect.custom` is the registered `Symbol.for('nodejs.util.inspect.custom')`.
Prototype data-method hooks receive the actual receiver, remaining depth, copied
options and the same inspect callback; getters are not executed to find a hook.
Self-returning hooks fall back to normal data inspection.

A shared nested budget limits inspection to 4096 nodes, 16,384 properties,
64 nesting levels, 256 custom hooks and 1 MiB output. Colors, hidden/proxy/getter
inspection, numeric separators, binary/Promise/internal Error-stack and other
advanced formatting remain explicit refusals. This is a partial formatter, not V8
introspection. Selected actual Node/Mite output and an Undici-shaped private-Map
custom inspector pass on the current macOS build; fresh platform acceptance remains
pending.

## deprecate(fn, message[, code])

Return a wrapper preserving receiver, arguments, result and constructor new.target,
prototype/statics and length. Invocation schedules a real deprecation warning;
uncoded wrappers warn once each, while a code is deduplicated across wrappers in
the same runtime. Dynamic noDeprecation suppression does not consume the first
warning. throwDeprecation/traceDeprecation follow the supported process warning
pipeline, including actual asynchronous exception delivery and captured stacks.

This is implemented behavior, not an import-only no-op. The focused Node/Mite
fixtures cover call/constructor identity, warnings and stderr, deduplication,
suppression, throws and traces. Fresh native Linux/Windows acceptance remains open.
See [process warnings](process.md#warnings) for limits and unsupported options.
