# Captured error stacks

Availability: Mite CLI and embedded main processes. The implementation captures
real LibJS frames; it does not embed V8. This is partial Node compatibility.

## Error.captureStackTrace(target[, constructorOpt])

Returns undefined and installs a non-enumerable, configurable stack accessor on
target. Extensible objects, arrays, functions and Error instances are accepted.
Primitives, Proxies, non-extensible targets and existing non-configurable stack
properties throw TypeError before mutation. Proxy rejection invokes no traps.

```js
class ProjectError extends Error {
  constructor(message) {
    super(message);
    this.name = 'ProjectError';
    Error.captureStackTrace(this, ProjectError);
  }
}
```

Frames are captured at the call. A function-valued constructorOpt removes frames
through its nearest actual call, using function identity. An absent function
produces a header-only stack; non-functions are ignored. Recapturing replaces the
old capture. Source paths, native frames and formatting remain LibJS-specific.
Async stack stitching and byte-for-byte V8 diagnostics are not provided.

The default formatter reads name/message on first stack access, then caches the
result. A thrown getter/conversion can be retried after correction. Assigning
stack caches any JavaScript value without coercion, including on a target frozen
after capture. Borrowed accessors use the receiver's captured record, or an
inherited capture holder; plain/primitive/Proxy receivers return undefined or
ignore assignments. Native Error receivers delegate to their existing engine
stack behavior.

## Error.stackTraceLimit

Default 10; writable, enumerable and configurable. For captured stacks, numeric
values are floored; zero, negatives and NaN collect no frames. Infinity includes
all available frames up to the native safety bound. Non-numeric data values or an
accessor disable capture, yielding undefined until assigned manually. Data values
can be inherited; accessors are not invoked. The limit is sampled at capture time.
More than 4096 requested-and-available frames fails explicitly with
ERR_MITE_UNSUPPORTED_API rather than silently truncating.

This setting governs captureStackTrace. Ordinary new Error/native thrown errors
retain LibJS's constructor/stack behavior; global V8-style stack limits and hooks
for all engine errors are not established. The fatal fallback reporter retains
its side-effect-free native trace rather than invoking arbitrary user accessors.

## Error.prepareStackTrace(error, callSites)

For an uncached captured stack, a callable hook runs lazily with this equal to the
original Error constructor, the actual capture target and a real CallSite array.
Its arbitrary return value is cached. Non-functions are ignored; hook/accessor
exceptions propagate without partial caching and permit retry. Cached/manual
values bypass later hook changes. Nested reads during a hook use the default
formatter, including reads of another captured target. Recursive name/message
formatting fails explicitly with ERR_MITE_UNSUPPORTED_API and can be retried after
fixing the accessor.

```js
Error.prepareStackTrace = (error, sites) => ({
  message: String(error),
  locations: sites.map(site => ({
    file: site.getFileName(),
    line: site.getLineNumber(),
    column: site.getColumnNumber()
  }))
});
```

Supported CallSite methods:

| Method | Result |
| --- | --- |
| getFunctionName() | Actual engine call name, or null when unnamed |
| getFileName(), getScriptNameOrSourceURL() | Captured engine filename, or null when absent |
| getLineNumber(), getColumnNumber() | Actual source coordinates, or null without a source range |
| isNative() | Whether this frame belongs to a LibJS native function; V8's classification can differ |
| toString() | Engine-specific source/name display without the stack line prefix |

Methods are shared, non-enumerable, writable and configurable. Borrowed methods
read the actual receiver's record; forged and Proxy receivers throw TypeError
without traps. Retaining sites is safe across GC and does not retain the capture
owner solely through source metadata.

V8-only or unavailable metadata methods throw ERR_MITE_UNSUPPORTED_API:
isConstructor, getThis, getFunction, getTypeName, getMethodName, getEvalOrigin,
isEval, isAsync, isToplevel, isPromiseAll, getPromiseIndex, getPosition,
getEnclosingLineNumber, getEnclosingColumnNumber and getScriptHash. In particular,
the current public LibJS ABI does not expose reliable general constructor-call
metadata; an interpreter fast-path flag is insufficient.

Acceptance: actual Node 24.15.0/Mite/embedded-host comparisons cover lazy access,
limits, real frame identity/locations, formatter reentry/retry, receiver branding
and caller-file discovery used by bindings-style packages. Node uses real exposed
GC; the embedded host separately verifies retention and idle reclamation of
capture owners, arbitrary cached values and retained CallSite metadata. CLI-only
checks do not claim forced GC. Fresh Linux/Windows execution remains open.
