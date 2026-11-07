# process

The Mite process object exposes native identity, environment, scheduling,
termination and timing services. It is available globally and through the same
default/CommonJS object from `process` and `node:process`. Firefly's main process
uses these services; Ladybird document JavaScript does not receive process.

**Availability:** partial. Native timing/CPU tests pass locally on macOS ARM64
against Node 24.15.0 and the non-GUI embedded host. Native Windows x64 and Linux
x64 Release gates at 02ed03f pass all 63 suites, including timing/CPU/GC and the
real time-span workflow. Existing broader runtime baselines have their
own [platform evidence](../compatibility/mite-0.1.md). This is not the complete
Node process API or Node identity emulation.

```js
import {hrtime, cpuUsage} from 'node:process';
const start = hrtime.bigint();
const before = cpuUsage();
// Perform application work here.
console.log(hrtime.bigint() - start, cpuUsage(before));
```

## Member index

| Members | Scope |
| --- | --- |
| `argv`, `argv0`, `execPath`, `pid`, `ppid`, `platform`, `arch` | Native process/script identity |
| `version`, `versions`, `release` | Mite/upstream versions; no versions.node claim |
| `cwd()`, `chdir(path)` | Native working directory and ordinary OS failures |
| `env` | Native environment proxy; string conversion for values, own-key enumeration and supported data descriptors |
| `exit([code])`, `exitCode` | Immediate exit or eventual runtime result; integer/string exit-code policy |
| `nextTick(callback, ...args)` | Rooted priority FIFO; see [timers/checkpoints](timers.md) |
| `hrtime([previous])`, `hrtime.bigint()` | Monotonic native elapsed-time samples |
| `uptime()` | Fractional seconds since runtime load/bootstrap |
| `cpuUsage([previous])` | Process-wide user/system CPU microseconds |
| EventEmitter methods | Custom events, listener order/removal/once and explicit native signal subset below |

Timing methods are shared between global process, CommonJS and named ESM exports.
Runtime/process identity is not an OS sandbox: main-process code is privileged.
Environment access retains platform-specific casing and OS rules. Enumeration,
modification and exit semantics are scoped by existing runtime tests; full Node
process property/descriptors/error-message parity is not certified.

## process.hrtime([previous])

Returns a fresh `[seconds, nanoseconds]` array from libuv's high-resolution
monotonic clock. The nanosecond remainder is in 0..999999999 for a raw sample.
The origin is arbitrary and unrelated to Unix time; clock resolution is platform
dependent even though the unit is nanoseconds. JS Date/wall-clock changes do not
affect these measurements.

Pass an earlier tuple to obtain an elapsed tuple. Subtraction borrows one second
when the nanosecond difference is negative. Non-arrays throw TypeError with
ERR_INVALID_ARG_TYPE; an array whose length is not two throws RangeError with
ERR_OUT_OF_RANGE. Legacy element arithmetic permits coercion; manually invented
tuples do not have a general normalized-result guarantee. Getter exceptions
propagate. Exact invalid-getter/reentrant coercion behavior is not certified.

### process.hrtime.bigint()

Returns the same native monotonic time in nanoseconds as a BigInt. Subtract two
results directly. Extra arguments are ignored; no previous-tuple argument is
needed. The native u64 total is split into an exact quotient/remainder before
converting to JS Numbers, then reconstructed with BigInt. It is never rounded
through a Number nanosecond total. Owned-policy tests include the u64 maximum;
actual native clocks are checked by bracketing tuple samples with BigInt samples.

Neither form retains timers, handles or VM objects after returning. Mutating a
tuple does not change later native samples. Values remain usable across GC.

## process.uptime()

Returns fractional seconds measured with a native steady clock. The baseline is
initialized once when the Mite runtime is loaded, before CLI/main VM execution;
it is not reset by creating a new realm. For an embedder loading Mite into an
already-running process, this is runtime-library lifetime, not the host's earlier
process lifetime. Startup/resolution values are not expected to equal Node's
bootstrap values. It is distinct from OS/system uptime in `node:os`.

## process.cpuUsage([previous])

Returns a fresh `{user, system}` record of native process CPU time in microseconds,
using libuv's uv_getrusage. It includes the process's threads/native workers, not
just the current JS task. Summed CPU time may exceed elapsed time on multiple
cores. Counter resolution depends on the host OS; short work can measure zero.

Pass an earlier record to subtract its fields. Falsy previous values are treated
as absent, matching the measured Node behavior. Truthy values require numeric
user/system fields in 0..Number.MAX_SAFE_INTEGER; fractional values are accepted.
Wrong types throw TypeError/ERR_INVALID_ARG_TYPE; negative, NaN, infinite or
excessive fields throw RangeError/ERR_INVALID_ARG_VALUE. A syntactically valid
future snapshot can produce negative differences; results are not clamped.

Valid getters are read for validation and again for subtraction; their exceptions
propagate. Native samples use fresh arrays, not a shared mutable JS scratch
buffer. Full invalid-getter/proxy/reentrant and error-message behavior is not a
general Node compatibility guarantee. Native OS failures throw with code, errno
and syscall uv_getrusage; provider failure injection is not native-acceptance-tested.

## Events and native signals

`process` inherits from the same [EventEmitter](events.md) exported by node:events.
Use `on`/`addListener`, `once`, `prependListener`, `prependOnceListener`,
`off`/`removeListener`, `removeAllListeners`, `emit` and listener inspection for
custom string or Symbol events. Listener methods return process; ordinary listeners
receive process as their receiver. These inherited methods are not named ESM
exports from node:process. Import the default object or use global process.

```js
process.on('SIGPIPE', (name, number) => {
  console.error(`Received ${name} (${number})`);
});
```

Native subscriptions are limited to SIGINT, SIGTERM, and SIGPIPE/SIGHUP where the
host SDK defines them, plus SIGUSR2 on POSIX. The callback receives the signal name
and actual native number. Multiple listeners share one unreferenced libuv watch;
a watch alone does not keep the application alive. Removing the final listener,
including automatic once removal, closes the watch. Signal callback exceptions
follow the existing runtime failure path; no uncaughtException recovery is supplied.

Native POSIX two-delivery/once/microtask/cleanup tests pass locally on macOS for
all five signals, against Node 24.15.0. A collecting embedded host also passes
SIGUSR2 delivery, foreign-handler refusal and restoration of an initially ignored
disposition after removal and runtime shutdown. Linux/Windows acceptance of this
new slice is pending. Windows registration does not establish POSIX-style signal
delivery: libuv can accept watches for signals that Windows never delivers.

Embedding hosts default to refusing native subscriptions with
ERR_MITE_SIGNAL_HOST_POLICY. A host may set HostHooks.allow_process_signals only
when it coordinates ownership of process-global signal handlers. The CLI and
Firefly executable explicitly opt in. An existing custom POSIX handler or another
Mite owner causes ERR_MITE_SIGNAL_OWNED; it is not overwritten. Previously ignored
dispositions are restored on teardown. This requires coordinated ownership; it
does not make concurrent, uncoordinated host changes atomic.

Known OS signals outside this subset throw ERR_MITE_UNSUPPORTED_SIGNAL; fault,
child-management and uncatchable signals are deliberately excluded. No more than
32 active or asynchronously closing native handles may be retained per runtime;
exceeding that bound throws ERR_MITE_SIGNAL_LIMIT until closes complete. A name
absent from the host signal constants remains an ordinary custom event.

Registering `beforeExit`, `exit`, `uncaughtException`, `uncaughtExceptionMonitor`,
`unhandledRejection`, `rejectionHandled`, `warning`, `message`, `disconnect`,
`multipleResolves`, `worker` or `workerMessage` throws
ERR_MITE_UNSUPPORTED_PROCESS_EVENT. These require real lifecycle/rejection/IPC
integration and are not simulated by an otherwise working emitter. process.kill
and arbitrary signal support remain absent. See
[ADR 0104](../architecture/0104-process-events-signals.md).

## Limits, example and validation

Process lifecycle/rejection events, stdio streams, broader signals, per-thread CPU usage,
memory/resource reports and broader performance profiling remain unfinished.
[node:perf_hooks](perf-hooks.md) now supplies native now/timeOrigin only. Absent members
produce ordinary API/import errors; they do not return fabricated measurements.
[Child processes](child-process.md) are a separate implemented subset.

The [modern process-metrics example](../../examples/mite-process-metrics/main.mjs)
hashes native bytes and reports independent elapsed/CPU/uptime measurements:

```sh
build/release-mite/bin/mite examples/mite-process-metrics/main.mjs
python3 tooling/tests/test_process_timing.py --mite build/release-mite/bin/mite --host build/release-mite/bin/mite-embedded-host
```

Tests use actual native measurements, timer waits, CPU work, wall-clock replacement
and forced collection. Separate owned-policy fixtures cover deterministic integer
boundaries/failure propagation; they are not OS measurements. The locked real
time-span package exercises the native BigInt clock through the ordinary ESM/npm
loader. Broader provider/platform/long-uptime/embedding coverage remains open.
Baseline: [Node 24.15 process](https://nodejs.org/download/release/v24.15.0/docs/api/process.html).
