# Mite 0.1 development compatibility

This is a versioned inventory, not a claim of general Node.js compatibility.
Statuses describe this repository, not upstream LibJS's language conformance.

| Surface | Status | Scope / known difference |
| --- | --- | --- |
| JavaScript scripts | partial | LibJS language engine; no Node CommonJS wrapper yet |
| CLI script arguments / `-e` | partial | File execution, eval, help/version; other flags fail explicitly |
| `console.log`, `console.error` | partial | String conversion and space joining; no Node object inspection or format substitutions |
| `process.argv` | partial | Script path and arguments; executable path normalization pending |
| `global` | partial | Alias to `globalThis` |
| Promise jobs | partial | Host FIFO queue and nested jobs; no full Node event loop yet |
| Unhandled rejections | partial | Nonzero exit after microtask drain; no process event API yet |
| ESM, CJS, npm package resolution | unsupported | Upcoming loader slice |
| Timers, `queueMicrotask`, `process.nextTick` | unsupported | Upcoming event-loop slice |
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
