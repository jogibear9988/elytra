# node:child_process

Partial native process support, locally tested on macOS ARM64 in standalone Mite
and the Firefly main process. The spawn/execFile suites also pass hosted Linux
and native Windows x64 at da5e3e8 (65-suite Release gates; [evidence](../status.md)), including the
disconnected-stdin finish and preserved write-EPIPE regression. Renderers do not
receive this privileged API.

```js
import {spawn, execFile, ChildProcess} from 'node:child_process';
```

require('child_process'), require('node:child_process'), the default ESM export and
named functions share identity. The [Node 24.15 reference](https://nodejs.org/download/release/v24.15.0/docs/api/child_process.html)
is the compatibility baseline; only the surface described here is implemented.

## spawn(file[, args][, options])

Start a native executable directly and return a ChildProcess. file must be a
nonempty string without NUL. args defaults to an empty array; arguments are passed
individually rather than interpreted by a shell. Do not concatenate a command line.
Node-shaped metadata preserves the public argument array while native arguments
are converted to strings. Prefer explicit string arguments in application code.

| Option | Current support |
| --- | --- |
| cwd | String or file URL; omitted/null/empty selects inherited cwd |
| env | Environment object, default current process.env; undefined values omitted; Windows duplicate keys handled case-insensitively |
| argv0 | Optional string override for the child's initial argument |
| stdio | pipe (default), ignore, inherit, or up to three entries using those modes / nonnegative inherited file descriptors |
| detached | Native libuv detached flag; does not itself unreference the process |
| timeout | Nonnegative integer through 0x7fffffff milliseconds; zero disables it |
| killSignal | Signal used by timeout; default SIGTERM |
| windowsHide / windowsVerbatimArguments | Native libuv flags; their Windows behavior still requires native acceptance |

Shell execution, IPC/additional stdio channels, stream-object stdio adoption,
AbortSignal, uid/gid and serialization options fail with ERR_MITE_UNSUPPORTED_API.
exec, fork and synchronous process APIs are absent. Direct construction
with new ChildProcess is also explicitly unsupported; use spawn.

## Process and stream lifetime

The returned object is an EventEmitter with pid, spawnfile, spawnargs, exitCode,
signalCode, killed, connected (false), stdin/stdout/stderr and stdio. Piped stdin is
a Writable; piped outputs are Readables with setEncoding, pause/resume and ordinary
stream operations. They provide ref/unref, but are not full net.Socket instances.
Input writes own their native byte snapshots until completion; output pressure
stops native reads. Consumers must drain or destroy large outputs to avoid blocking
the child. Unobserved small output is drained after process exit.

Ending stdin after its peer has closed succeeds without an ENOTCONN stream
error. As in [Node 24.15 net.Socket finalization](https://github.com/nodejs/node/blob/v24.15.0/lib/net.js),
asynchronous shutdown completion finishes the stream; other immediate shutdown
errors remain errors. Writes to a closed peer still report EPIPE. Native tests
coordinate an actual Python child closing stdin before the parent ends/writes;
separate labeled fault-injection tests cover completion/error-policy boundaries.

On POSIX, the Mite CLI and Firefly executable ignore SIGPIPE at startup so native
pipe failures reach JavaScript instead of terminating the process. Mite::run does
not change an embedder's signal dispositions. Hosts that use native pipe/socket
writes must establish an appropriate signal policy themselves; the optional
Mite::ignore_broken_pipe_signal helper is an executable-startup choice. This does
not implement JavaScript process signal listeners.

spawn is emitted asynchronously on success. exit reports code/signal; close waits
for process monitoring and pipe closure. Output can continue after exit. Missing
executables report error asynchronously with native code/errno, syscall, path and
spawnargs; no successful spawn/exit event is fabricated. Argument validation is
synchronous. Handle process errors and any stream errors relevant to your workflow.
All native error timing/stdio edge cases and allocation/provider failures are not
certified as Node-identical.

kill([signal]) returns success and updates killed; this does not prove termination.
ref/unref control the process handle and return undefined. Pipe references are
independent. [Symbol.dispose] requests default termination. Signal semantics depend
on the OS. A timeout sends its configured signal; it does not guarantee immediate
termination. Parent runtime teardown closes monitoring/pipes without automatically
killing the independent child, matching the tested detached/unreferenced workflow.
Process groups, orphan reaping in arbitrary embedding hosts and descendants are not
a certified process-supervision facility.

The Windows CLI establishes a non-inheritable policy for its inherited standard
handles at startup. Explicit stdio:'inherit' still passes the requested streams.
This is intended to stop an ignored-stdio descendant from accidentally keeping
the CLI's capture pipes open; the scoped native Windows regression passes in the
74bce40 gate. Arbitrary process-supervision and embedding policies remain separate.
Embedded hosts own their process-wide inheritance policy. See the
[Windows build notes](../development/windows-build.md#cli-stdio-inheritance-and-detached-child-diagnostics)
for scope and separate process-exit/pipe-EOF diagnostics.

## execFile(file[, args][, options][, callback])

Run an executable and collect stdout/stderr separately. The optional callback
receives (error, stdout, stderr); the function returns the live ChildProcess so
applications can write its stdin or request termination. Callbacks run
asynchronously and once, including failed starts. A nonzero exit produces an
error with code, killed, signal and cmd; start errors retain native errno,
syscall, path and spawnargs. Native error text/stack fidelity remains incomplete.
The callback normally follows exit after output closure; a failed start can call
back during the error event before close. Omitting the callback still runs the
child and enforces the output/timeout policies.

Default encoding is utf8, with incremental stream decoding across split byte
sequences. encoding:'buffer', null and unrecognized encodings return Buffers,
following the reference behavior. Supported Buffer encodings such as hex also
work. maxBuffer defaults to 1 MiB **per output stream**; Infinity explicitly
disables the limit. Overflow destroys output streams, requests termination and
reports ERR_CHILD_PROCESS_STDIO_MAXBUFFER with retained output. The reference's
decoded-string truncation uses code-unit slicing with a remaining byte budget;
returned Unicode text can therefore encode to more bytes than maxBuffer. Use
encoding:'buffer' for an exact byte prefix. Termination is a request, not a
guarantee that an uncooperative process or its descendants have exited.
Explicit maxBuffer:undefined also disables comparison, while null behaves as zero,
matching the tested reference; omit the property to retain the default limit.

cwd, env, timeout, killSignal, windowsHide and windowsVerbatimArguments use the
native spawn machinery. Timeout zero disables the timer; values above 0x7fffffff
fail explicitly. Shell execution, AbortSignal and privilege switching remain
unsupported. exec, fork, synchronous variants and util.promisify integration are
not implemented. Extra stdio/detached/argv0 behavior from spawn is not part of the
execFile contract; this API always creates the three capture/input pipes.

```js
const child = execFile('/path/to/tool', ['argument'], {maxBuffer: 65536},
  (error, stdout, stderr) => {
    if (error) console.error(error.message);
    else console.log(stdout);
  });
child.stdin.end();
```

## Examples and verification

The [standalone example](../../examples/mite-cli/spawn.mjs) starts another copy of
the current CLI, reads its pipe and awaits close. Run it under Mite or Node.
Firefly's executable is an application launcher, so spawning it with CLI eval
arguments is not the same workflow.

mite-child-process compares Node/Mite with independent Python children: Unicode
arguments/cwd/environment, 1 MiB echo with write pressure/paused reads, separate
stderr, nonzero exit, start/cwd failures, inherited/ignored pipes, argv0, kill,
timeout, concurrent children, unconsumed output, validation, fatal callbacks and
detached/unreferenced parent exit. Python is a test peer, not a shipped runtime
dependency. The Firefly ESM fixture also starts a real native process and reads its
pipe alongside normal renderer/crash/lifecycle checks. See
[ADR 0062](../architecture/0062-native-child-processes.md).

The [buffered CLI example](../../examples/mite-cli/exec-file.mjs) uses execFile
with the current runtime. mite-exec-file compares captured text/binary/hex output,
split Unicode decoding, exit/start/cwd errors, stdout/stderr/default/zero/fractional/
Unicode byte limits, 1 MiB stdin, timeout, callback overloads/absence/count/order,
29 validation cases and fatal callback behavior against Node using independent
Python peers. The Firefly main fixture executes and captures a real printf child.
See [ADR 0066](../architecture/0066-buffered-native-exec-file.md).


## Promisified execFile

[util.promisify](util.md) recognizes execFile's custom implementation. Its Promise
has an immediate .child property and resolves with {stdout, stderr}. Rejection
errors retain both output fields. The same callback implementation still owns
spawning, bounded collection, cancellation and completion; options/platform limits
above still apply. Local macOS Node/Mite tests execute real children and verify
both successful and nonzero output paths; native Linux/Windows also pass the
promisify gate at a56d183. See [current evidence](../status.md).
