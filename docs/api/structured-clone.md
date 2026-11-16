# structuredClone

Status: partial standalone global API. Local macOS ARM64 comparisons pass against
Node 24.15, including the actual pinned xterm headless terminal. Fresh native
Windows/Linux acceptance remains unverified. Mite uses its own LibJS graph copy;
it does not require Node, V8, Firefly or LibWeb.

```js
const original = { values: new Uint8Array([0, 128, 255]) };
original.self = original;
const copy = structuredClone(original);
copy.values[0] = 42;
console.log(copy.self === copy, original.values[0]); // true, 0
```

## structuredClone(value[, options])

Returns a synchronous, independent, mutable copy. Supported values include
primitives (including undefined, BigInt, NaN and negative zero), ordinary objects,
sparse arrays, Date, RegExp, Map, Set, ArrayBuffer, intrinsic typed arrays and
DataView. Cycles and repeated references retain their relationships within the
copy. Views sharing one buffer share a new copied buffer with their original
offsets/lengths. Buffer becomes Uint8Array. RegExp lastIndex resets to zero.

Ordinary/custom-class instances copy enumerable own string-keyed properties and
normalize their prototype to Object.prototype. Array holes are retained. Getters
run once when their property is copied; thrown exceptions propagate unchanged.
Inherited/non-enumerable/symbol properties are not copied. Input state is never
frozen or detached. Intrinsic binary/collection access ignores spoofed instance
getters, and private native brand checks are independent of mutable util.types.

Calling without a value throws TypeError/ERR_MISSING_ARGS. Options may be omitted
or null. Invalid options/transfer iterables throw TypeError/ERR_INVALID_ARG_TYPE.
An empty transfer iterable is accepted. Any nonempty transfer iterable throws
ERR_MITE_UNSUPPORTED_API before traversing the input or detaching storage.

## Unsupported values and bounds

Functions, symbols, Proxies, weak collections, Promises, native errors, opaque
host objects and unsupported engine objects fail with DataCloneError. Shared and
resizable buffers are unsupported. Blob/File, URL/search parameters, streams,
fetch objects and cancellation controllers/signals are not silently converted to
empty objects, including after prototype replacement. Objects whose prototype
chain contains a Proxy also reject without invoking its traps. Boxed primitives
and other unlisted engine types are not accepted by this subset.

The copy has explicit bounds: depth 128, 65,536 distinct objects, 131,072
properties/collection entries, 65,536 array length, four Mi UTF-16 string code
units (including property names), and one MiB of unique backing-buffer bytes.
Map keys and values each consume one property unit. BigInt decimal text is
charged to the string budget. Limit failures throw DataCloneError. Node's global
API does not impose these same quotas.

This API copies within one Mite realm. It does not provide cross-process file,
port, shared-memory or zero-copy transfer. The separate worker and Firefly IPC
contracts retain their own ownership and limits.

## Validation

Seven Node/Mite groups cover graph identity, sparse arrays, scalar preservation,
getter/prototype behavior, collections, binary backing aliases, capability
refusal and transfer differences. Mite-specific cases verify private brands after
public predicate poisoning and prototype replacement. The optional --stage
fixture verifies the original xterm package/entry SHA-256, creates its real
Terminal, writes ANSI text asynchronously, inspects the buffer, resets and
disposes it. Without that stage, CI explicitly reports the package fixture skipped.
Terminal component success does not establish a working VS Code editor or PTY.

Reference: [Node structuredClone](https://nodejs.org/download/release/v24.15.0/docs/api/globals.html#structuredclonevalue-options).
