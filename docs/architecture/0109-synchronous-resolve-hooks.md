# 0109: Realm-owned synchronous module resolution hooks

Implement the public node:module.registerHooks resolve contract against the actual
native ESM and CommonJS resolvers. Code-OSS imports it in bootstrap-esm.ts; adding
a throwing/no-op named export would hide an API gap. Resolve-only registration is
a useful working slice; load hooks remain explicitly rejected at registration.

Keep chain ordering, registration/deregistration and continuation closures in an
owned ESM module. This is ordinary engine-independent application logic, bundled
with existing build-only tooling. C++ remains responsible for native file/package
resolution, URL validation, module identity and loading. No Rust/FFI boundary or
Node/V8 runtime is introduced. Pass package conditions explicitly down the native
call graph rather than using mutable process-wide resolver state.

The loader roots one dispatcher in its VM/realm. The dispatcher owns active hook
records and snapshots a chain for each traversal. Removal affects later calls;
in-flight traversals retain their snapshot. Continuations capture JS state and a
persistent native resolver function, never a borrowed native stack address. They
remain callable after the original resolution returns and after deregistration.
Forced host GC is tested both with registered hooks and a retained continuation.

Hooks run for later static edges/dynamic imports, require and require.resolve,
including createRequire contexts. Canonical URL/CommonJS cache identities remain
native. A hook must continue or explicitly short-circuit; results must be
synchronous objects with absolute URLs. Validate final URL schemes/hosts/encoded
separators/NUL, registered host existence, matching format hints and effective JSON
attributes. Private CJS/builtin/host bridge imports retain their native shim checks.
Hooks grant no renderer/preload loader capability.

Limits are explicit: at most 64 active resolve hooks and 1,024 conditions;
module-sync remains absent while synchronous require(ESM) is unsupported. No
load hooks, source/format transforms, asynchronous register, ASAR filesystem,
non-file custom protocols or worker inheritance. General import-attribute cache
variants are not added; only the existing JSON type contract is supported. Error
messages/codes for some malformed hook return types differ from Node.

Actual Node 24.15/Mite tests cover LIFO chains, both loaders, cache identity,
conditions, changed bases, builtins, static graph edges, reentry, explicit JSON
attributes, invalid results, exact thrown exceptions and retained continuations.
Local macOS all 76 CTests pass; native hook acceptance is pending. The full-source
Firefly probe now passes the registerHooks import and fails on missing util.promisify
(exit 1). This does not prove Code-OSS registers/executes its Electron-only ASAR hook
under truthful Mite identity or that the workbench runs. Ladybird stays unmodified.

References: [Node 24.15 hook contract](https://nodejs.org/download/release/v24.15.0/docs/api/module.html#customization-hooks),
[API limits](../api/module.md#registerhooksoptions) and
[Code-OSS priorities](../compatibility/vscode.md).
