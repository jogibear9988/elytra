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
| `title` | Native main-process title; ordinary worker-local metadata |
| `version`, `versions`, `release` | Mite/upstream versions; no versions.node claim |
| `cwd()`, `chdir(path)` | Native working directory and ordinary OS failures |
| `env` | Native environment proxy; string conversion for values, own-key enumeration and supported data descriptors |
| `exit([code])`, `exitCode` | Immediate exit or eventual runtime result; integer/string exit-code policy |
| `nextTick(callback, ...args)` | Rooted priority FIFO; see [timers/checkpoints](timers.md) |
| `hrtime([previous])`, `hrtime.bigint()` | Monotonic native elapsed-time samples |
| `uptime()` | Fractional seconds since runtime load/bootstrap |
| `cpuUsage([previous])` | Process-wide user/system CPU microseconds |
| `stdout`, `stderr` | Writable wrappers around borrowed descriptor 1/2; native descriptor-specific writes |
| EventEmitter methods | Custom events, listener order/removal/once and explicit native signal subset below |

Timing methods are shared between global process, CommonJS and named ESM exports.
Runtime/process identity is not an OS sandbox: main-process code is privileged.
Environment access retains platform-specific casing and OS rules. Enumeration,
modification and exit semantics are scoped by existing runtime tests; full Node
process property/descriptors/error-message parity is not certified.

## process.title

The main-process property reads and changes the actual OS process title through
libuv. Assignment performs ordinary string conversion before changing native
state; conversion exceptions propagate. OS restrictions still apply: titles can
be truncated to the platform's available argument storage, and display in process
tools varies by OS. This does not set a Firefly window title or application name.
Embedded NUL follows libuv's C-string behavior. UTF-8 decoding replaces invalid
bytes without dropping a leading BOM. Inputs must fit below one MiB of UTF-8;
oversized values fail with `ENOBUFS`. Getter buffers grow to the same bound and
report native errors rather than returning a partial result.

In a worker, `title` starts as a snapshot of the containing process's native
title. It is an ordinary writable local property: assigning an object preserves
that object and does not invoke string conversion or rename the OS process.
Reads/writes of native title state are synchronized across Mite's threads.

The main property is an enumerable, configurable accessor. Node exposes a data
descriptor backed by native property interception; descriptor parity is an
intentional difference. Mite also preserves the previous native title when
conversion throws, while the tested Node 24.15 setter clears it. Replacing Mite's
configurable accessor with another descriptor replaces its native behavior.
Global/default/CommonJS process share the same object; named ESM exports are
initial snapshots, not live native title subscriptions.

Reference: [Node process.title](https://nodejs.org/download/release/v24.15.0/docs/api/process.html#processtitle).

Local macOS ARM64 CLI and embedded-host checks cover native reads/writes,
coercion and errors, worker-local objects, a real separate child process and
restoration. Fresh Linux/Windows title acceptance remains unverified. The
unchanged pinned xterm package now gets past its process-title-based runtime
detection; constructing its Terminal currently requires the still-missing global
`structuredClone`. This is not terminal or VS Code acceptance.

## Standard output

process.stdout and process.stderr are real Writable streams using borrowed native
OS descriptors 1 and 2. Global/default/CommonJS and named process exports share
them. Writable converts strings to their requested encoding and preserves binary
bytes, FIFO, backpressure, write callbacks and application write hooks.

The native transport selects synchronous writes for files on all platforms,
POSIX terminals and Windows pipes. POSIX pipes/sockets first attempt immediate
writes; only an unwritten suffix creates a copied libuv write request. Windows
terminals use a libuv stream; Windows TCP-backed stdio explicitly rejects the
unsupported CRT/socket conversion. No output is queued in the filesystem worker
pool. A completed small write creates no native loop reference; actual pending
requests retain liveness. Native limits are 2 MiB per chunk and 8 MiB retained
pending bytes per runtime; excess writes report ENOBUFS.

Descriptors 1/2 remain borrowed. Runtime teardown closes owned duplicates and
restores shared nonblocking flags after the final runtime/native handle retires.
stdout/stderr aliases share that lease. Active asynchronous pipes share their
nonblocking state with the host; embeddings must keep standard descriptors stable
while runtimes are active. Supported process-ending paths also restore flags,
without flushing pending data or driving application callbacks. SIGKILL/crash
cleanup is not guaranteed. See [ADR 0207](../architecture/0207-native-standard-output.md).

end()/destroy() retire the JS wrapper without closing the host's descriptor or
shutting down its shared pipe/socket. An already submitted write may still finish.
This differs from Node's special non-ending standard streams. process.exit()
remains immediate and may truncate genuinely pending asynchronous output; await
callbacks for required output. Public stdout TTY metadata/control/ref-unref and
process.stdin remain separate work. Reading process.stdin explicitly throws
ERR_MITE_UNSUPPORTED_API; it is not a named ESM export.

Actual local macOS Node/Mite controls pass binary/UTF-8 bytes, short writes,
callbacks/EOF, small-write unref behavior, regular files, slow pipe backpressure
and exact suffix/FIFO, closed-pipe EPIPE callback/error identity, separate/aliased
borrowed flag restoration and parent-descriptor survival. The genuine embedded
host also passes separate/aliased restoration. Exit/immediate/abort/process-error/
DNS regression suites and Console write-hook/boundary controls pass. Evidence:
build/native-stdio-{controls,regressions,console-controls,builtin-controls}.log.
Fresh native Linux/Windows stdio acceptance and full Node stdio parity remain open.

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
use the [native process exception path](process-errors.md).

Native POSIX two-delivery/once/microtask/cleanup tests pass locally on macOS for
all five signals, against Node 24.15.0. A collecting embedded host also passes
SIGUSR2 delivery, foreign-handler refusal and restoration of an initially ignored
disposition after removal and runtime shutdown. Native Linux x64 also passes at
94acc0c ([run 42380475178](https://github.com/jogibear9988/elytra/actions/runs/42380475178),
all 69 suites). Windows x64 passes registration/unref/policy at 94acc0c
([run 42380475051](https://github.com/jogibear9988/elytra/actions/runs/42380475051),
all 69 suites). Windows registration does not establish POSIX-style signal
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

Native uncaughtException, uncaughtExceptionMonitor, unhandledRejection and
rejectionHandled events now have real event-loop delivery. See the detailed
[exception/rejection reference](process-errors.md) for identities, callback and
entry-point origins, default exits, listener failures and GC lifetime.

Registering beforeExit, multipleResolves, worker or workerMessage still
throws ERR_MITE_UNSUPPORTED_PROCESS_EVENT. message/disconnect also reject outside
an owned fork child. Process kill, arbitrary signals, capture callbacks and full
lifecycle coverage remain unfinished. [ADR 0104](../architecture/0104-process-events-signals.md)
records the initial signal scope; [ADR 0182](../architecture/0182-native-process-error-events.md)
describes the native error-event integration.

The separate pinned Code-OSS source-mode gate evaluates its real bootstrap-node
module, including this SIGPIPE subscription and native cwd/environment setup,
then exercises portable configuration. That confirms one bootstrap dependency
path, not the full Code-OSS main entry or desktop application.

## Limits, example and validation

Process lifecycle/rejection events, stdin/full stdio semantics, broader signals, per-thread CPU usage,
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

## Exit notification

A synchronous exit event now runs once before orderly runtime/host teardown and
before explicit process.exit. It receives the selected code, leaves ordinary
undefined exitCode unchanged for a successful natural exit, and observes listener
updates to exitCode. process.exit(undefined) clears a previously selected code;
omitting the argument retains it. A nested process.exit immediately terminates
without a second emission or remaining listener callbacks. Throwing listeners
propagate; uncaught failures default to code 1.

No timer, nextTick or asynchronous native work is driven after the terminal
notification. Mite also does not drain new Promise reactions then; Node 24 can run
those at natural (but not explicit) exit, an intentional current difference.
New blocking work cannot extend terminal lifetime. Hosts already stopping with
pending work retain their explicit pending-work policy. This is not beforeExit,
rejection/exception recovery or a cancellable exit API.

[Eight real Node/Mite exit cases](../../tooling/tests/test_process_exit.py) pass
locally on macOS. Fresh Windows/Linux acceptance remains required. These callbacks
are sufficient for the actual Code-OSS IPC client's child cleanup; they do not
establish complete process lifecycle parity.

`process.execArgv` now reports an empty array for file execution or the actual
eval/print flag/source pair for -e/--eval or -p/--print. Internal fork role flags are absent. Fork-child
message/disconnect events, send/connected/channel exist only with the owned child
capability; ordinary processes cannot enable it by assigning public properties or
an environment marker. process.parentPort is not installed by Node-style fork.

## Warnings

`process.emitWarning(warning[, type[, code[, ctor]]])` and its options-object form
schedule a genuine nextTick `warning` event. Strings create real Error objects;
an authored Error retains its identity. Supported options are type, code, detail
and ctor. The installed default warning listener writes to actual stderr. Dynamic
noDeprecation, throwDeprecation, traceDeprecation and traceProcessWarnings control
the supported warning path; a deprecation throw is asynchronous after the caller.
This does not add unsupported command-line warning flags.

Pending warnings are limited to 256 and 1,048,576 text code units; each text field
is at most 65,536 code units. Limit/unsupported-option errors are explicit.
Focused Node/macOS Mite fixtures pass actual warning events, stderr, Error identity,
trace stacks and asynchronous throws. See [util.deprecate](util.md).
