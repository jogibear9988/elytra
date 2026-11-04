# Mite 0.1 development compatibility

This is a versioned inventory, not a claim of general Node.js compatibility.
Statuses describe this repository, not upstream LibJS's language conformance.

| Surface | Status | Scope / known difference |
| --- | --- | --- |
| JavaScript file entry | partial | LibJS engine with CommonJS wrapper or explicit ESM format; no automatic ESM syntax detection |
| CLI script arguments / `-e` | partial | File execution, eval, help/version; other flags fail explicitly |
| `console.log`, `console.error` | partial | String conversion and space joining; no Node object inspection or format substitutions |
| `process.argv` | partial | Script path and arguments; executable path normalization pending |
| `global` | partial | Alias to `globalThis` |
| Promise jobs / `queueMicrotask` | partial | Shared FIFO queue, nested jobs, checkpoint after each native callback; no process.nextTick or rejection events |
| Unhandled rejections | partial | Nonzero exit after microtask drain; no process event API yet |
| ES modules | partial | `.mjs`, `package.json` type=module, relative/absolute file URLs, static/dynamic imports, live bindings, cycles, URL cache identity, import.meta url/filename/dirname/main and top-level await tested; no syntax auto-detection or import.meta.resolve yet |
| JSON modules | partial | Default exports with type=json import attributes, cache identity, mutations and syntax errors tested; unknown attribute names are rejected by LibJS before the host and their error codes differ from Node |
| CommonJS file modules | partial | Wrapper, require/resolve/cache/main, cycles, exports replacement, metadata, cache invalidation/retry, relative and directory main/index loading tested; eval globals, require.extensions, resolve options and synchronous require(ESM) unsupported |
| ESM/CJS interoperation | partial | Lazy default and module.exports namespace exports, shared JSON cache tested; static named CJS export detection unsupported |
| npm package resolution | unsupported | Next loader slice; bare package names fail explicitly |
| `setTimeout`, `setInterval`, clearing timers | partial | Native libuv timers; arguments, callback receiver, numeric/string cancellation, ref/unref/hasRef/close/dispose tested; refresh and timeout warnings unsupported |
| `setImmediate`, `process.nextTick` | unsupported | Additional Node event-loop phases pending |
| `Buffer`, URL, encoding, fetch | unsupported | Not installed as globals |
| Node built-in modules | unsupported | Not implemented |
| Native addons, N-API, V8 internals, `process.binding()` | unsupported | Explicitly outside initial scope |

Missing APIs are absent and therefore raise JavaScript errors when used. There
are no success-returning placeholder implementations. The first fixture suite
compares stdout and exit status with Node, while requiring stderr for failures;
stack-trace formatting does not yet match Node. Native platform validation and
actual test results are recorded in `docs/PROGRESS.md`.

Behavior references: [CLI](https://nodejs.org/api/cli.html),
[globals](https://nodejs.org/api/globals.html),
[process](https://nodejs.org/api/process.html). Node is only a test reference and
is never linked into or invoked by Mite.
