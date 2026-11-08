# node:assert and node:assert/strict

Status: **partial**, locally tested on macOS against Node 24.15.0 and the native
non-GUI GC host. Fresh Linux/Windows execution remains unverified. Mite executes
pinned portable JavaScript on LibJS; no Node/V8 runtime is embedded.

## Imports and common assertions

```js
import assert from 'node:assert';
import strict from 'node:assert/strict';

assert(true, 'invariant failed');
assert.equal(1, '1');
strict.equal(1, 1);
strict.deepEqual({ names: ['Mite'] }, { names: ['Mite'] });
assert.throws(() => { throw new TypeError('bad input'); }, TypeError);
await assert.rejects(async () => { throw new Error('failed'); }, /failed/);
```

Bare assert and assert/strict imports and CommonJS require share the same per-loader
objects as their node: aliases. The default assert function is identical to assert.ok.
assert.strict is the strict module, whose equal/notEqual/deepEqual/notDeepEqual
aliases use strictEqual/notStrictEqual/deepStrictEqual/notDeepStrictEqual. Root
strictEqual and notStrictEqual use Object.is semantics, including NaN and signed zero.
Root equal/notEqual retain coercive behavior, with modern Node NaN equality. fail() and fail(message) throw;
an Error supplied as the message is propagated by identity where applicable.

The default supported surface includes ok, fail, equal, notEqual, strictEqual,
notStrictEqual, deepEqual, notDeepEqual, deepStrictEqual, notDeepStrictEqual,
throws, doesNotThrow, rejects, doesNotReject, ifError, match, doesNotMatch and
AssertionError. Missing required equality arguments reject rather than silently pass.
ifError accepts only null/undefined. match/doesNotMatch require string/RegExp inputs.
Only the enumerated/tested subset is claimed, not complete current Node coverage.

## Equality and errors

Supported deep cases include primitives, ordinary enumerable data properties,
arrays/sparse arrays, cycles, symbol keys, prototype distinctions in strict mode,
valid Dates, RegExp with zero lastIndex, basic Errors, Maps/Sets, ArrayBuffer,
DataView, integer typed arrays/Buffer and supported boxed primitives. Legacy loose
comparison retains its coercive rules. Exact graphs/collection matching beyond the
accepted cases remain partial.

Known unverified modern cases fail with ERR_MITE_UNSUPPORTED_API before a successful
deep comparison can be reported: Promise/weak collections/WeakRef and unknown branded
objects, Float/BigInt typed arrays and boxed BigInt, invalid Dates, stateful RegExp,
Error cause/AggregateError state, custom own comparison tags and enumerable accessors.
Traversal is limited to 10,000 distinct objects and depth 256. Identical top-level
objects retain identity semantics. Exception object patterns also check guarded
comparison values before reporting success. These restrictions apply to both
positive and negative deep assertions; unsupported values never count as proof of
inequality. Do not use this partial API as a general equality/security boundary.

AssertionError is a real Error subclass with code ERR_ASSERTION and the tested
actual, expected, operator, message and generatedMessage fields. Explicit strings,
Error message identity, missing-argument errors and core failure classifications
are tested. Automatically generated messages, value inspection, source snippets,
stack trimming, colors and exact diagnostic formatting differ from modern Node.
The portable provider's own inspection implementation is bundled privately;
Mite's public node:util gains no inspection/type APIs from this work.

## Exceptions and Promises

throws accepts a function and optional Error constructor, RegExp, validator or
supported object pattern. rejects supports a current-realm native Promise or a
function returning one, with the same matcher subset. Successful validators return
true. Matcher mismatches throw/reject AssertionError with the original actual error
and expected matcher; a validator's own exception propagates unchanged. A synchronous
throw inside the function passed to rejects remains that rejection, rather than
being mistaken for a matched asynchronous rejection. Non-native thenable rejection
validation is explicitly unavailable. The broader provider thenable/foreign-realm
behavior is not advertised.

doesNotThrow/doesNotReject retain their tested no-error/unwanted-error contracts.
Constructor/predicate/accessor/proxy/foreign-realm and diagnostic parity outside the
verified cases remains partial. Modern Assert instances/options, CallTracker,
partialDeepStrictEqual and the deprecated multi-argument fail overload explicitly
throw ERR_MITE_UNSUPPORTED_API. Other missing Node exports are not advertised.

## Provenance and verification

The basis is assert 2.1.0 (MIT, Node contributors) and its exact locked portable
dependencies. Three checked in-memory build adaptations correct the older provider's scalar/deep
loose NaN rules without modifying installed package files. The generator records
all package/source/license hashes, includes
notices in the embedded bundle and distributes assert.LICENSE.txt through existing
packagers. Its graph has no external runtime imports. Upstream Ladybird is unchanged.

The mite-assert CTest compares real Node/Mite outcomes and operator/error metadata,
callable/strict/CJS/ESM identities, deep structures, synchronous/asynchronous failure
paths and native GC retention. Separate cases prove the explicit unsupported rules.
The actual pinned Code-OSS main/CLI argument validator also passes local Node/Mite
valid and invalid --goto inputs. This is component evidence, not a workbench launch.
See [ADR 0152](../architecture/0152-node-assert.md).

References: [Node assert API](https://nodejs.org/docs/latest-v24.x/api/assert.html),
[portable provider](https://github.com/browserify/commonjs-assert).
