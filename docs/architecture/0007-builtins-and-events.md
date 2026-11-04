# ADR 0007: shared builtin registry and EventEmitter

Status: events slice implemented and tested on macOS ARM64 against Node v24.15.0.

Keep module resolution results distinct from filesystem paths for builtins. A
per-VM registry owns the builtin export objects returned by CommonJS. Canonical
node: module identities own native ESM shims with a fixed export list. Both import
forms share the same constructor/function objects. Bare builtin cache overrides
retain Node behavior, while a node: prefix bypasses require.cache overrides.
Known names are recorded from the Node v24.15.0 public module.builtinModules list;
that inventory is not a claim that all those modules are implemented.

Implement pure API semantics in embedded JavaScript running on LibJS, with C++
for module lifecycle, GC roots and native services. This avoids duplicating
JavaScript listener/callback semantics in C++ or introducing a Rust FFI. Sources
are repository assets embedded at build time; no installed Node library is used
or invoked. The registry descriptor declares the source and named ESM exports.
Native services can later supply objects through the same registry.

EventEmitter implements synchronous ordered dispatch, prepend/once, snapshots
during reentrant mutation, raw/original listener inspection, removal hooks,
symbol names, listener counts/limits, error events/errorMonitor, and the Promise
once helper. Its constructor supports both ES class inheritance and legacy
EventEmitter.call(this). Shared identity and CommonJS cache bypass are tested.

Only trusted Mite main-process code installs this registry. The builtin source
receives a private host object with a stderr warning writer. Internal ESM bridge
imports are accepted only from host-created shim records. This is not a renderer
capability mechanism; Firefly must use a separate constrained bridge.

Incomplete behavior is explicit: captureRejections=true and once AbortSignal
options reject with ERR_MITE_UNSUPPORTED_API. EventTarget, async event iterators,
async hooks/EventEmitterAsyncResource and addAbortListener are absent. Named
builtin exports are initial values; syncBuiltinESMExports is not implemented.
MaxListeners warnings are emitted, but currently use a microtask and a simplified
stderr header, without process warning events/PID/stack formatting. Warning tests
check the diagnostic marker and listener behavior, not exact Node formatting or
nextTick ordering. These gaps prevent a general events compatibility claim.

References: [Node events](https://nodejs.org/api/events.html),
[Node builtin modules](https://nodejs.org/api/module.html#modulebuiltinmodules).
