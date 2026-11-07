# node:perf_hooks

Status: partial native clock API. Local macOS ARM64 Node 24.15 comparisons and
actual embedded-host collection pass. New Linux/Windows execution remains pending.
This does not implement Node's complete performance measurement API.

```js
import {performance} from 'node:perf_hooks';
import {setTimeout} from 'node:timers/promises';

const start = performance.now();
await setTimeout(25);
console.log(performance.now() - start); // elapsed milliseconds
console.log(performance.timeOrigin);   // native epoch milliseconds at runtime origin
```

## performance.now()

Return nonnegative monotonic elapsed milliseconds, including fractional values,
from the native runtime's initial steady-clock sample. It shares the origin used
by process.uptime. It does not use JavaScript Date, the current OS wall clock,
per-import initialization or a JavaScript timer counter. Clock precision and
resolution depend on the native platform; no accuracy or performance comparison
with Electron is claimed.

## performance.timeOrigin

A read-only epoch timestamp in milliseconds sampled once alongside the runtime's
initial monotonic sample. It remains fixed after import, asynchronous work,
collection and later wall-clock changes. This is the runtime initialization origin,
not a separately queried kernel process-creation timestamp. now() always uses the
steady clock; timeOrigin + now() approximates epoch time without subsequent wall
clock adjustments.

## Identity and receivers

The global performance, named performance export and performance property of the
default perf_hooks/node:perf_hooks module refer to the same object. CommonJS
createRequire sees the same cached exports. Replacing the writable global does
not replace the module's original clock.

now and the timeOrigin getter require the clock's own receiver brand. Invalid,
missing or inherited receivers throw TypeError with ERR_INVALID_THIS. Transparent
proxies follow Node's own-brand rule; a revoked proxy throws its native TypeError.
Assignment to timeOrigin fails in strict mode. These public compatibility receiver
checks are not a security boundary or a source of privileged process access.

## Remaining APIs and acceptance

The facade supplies now/timeOrigin only. Performance constructors, timeline
marks/measures, resource entries, nodeTiming, event-loop utilization, observers,
histograms, timerify and profiling are absent. There are no fabricated measurements
or successful empty profiling operations. Global property descriptors and the
complete Performance/EventTarget prototype hierarchy are not Node-compatible yet.

The gate also executes the actual pinned Code-OSS common/performance.ts module
after development-only Node type erasure. Its origin/startup marks, explicit
timestamp, reads and clearing run in both Node and Mite; Code-OSS itself supplies
its existing Node-style mark fallback. This does not mean Mite implements
performance.mark, nor that mainImpl, the workbench or the full bootstrap runs.
The unmodified test source, commit/hash manifest and MIT license are retained in
vendor/code-oss-tests. No Node process is embedded or needed to run Mite apps.

References: [Node 24.15 performance API](https://github.com/nodejs/node/blob/v24.15.0/doc/api/perf_hooks.md),
[pinned Code-OSS performance module](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/src/vs/base/common/performance.ts).
