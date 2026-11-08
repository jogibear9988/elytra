# node:child_process

Partial native process support, locally tested on macOS ARM64 in standalone Mite
and the Firefly main process. The spawn/execFile suites also pass hosted Linux
and native Windows x64 at da5e3e8 (65-suite Release gates; [evidence](../status.md)), including the
disconnected-stdin finish and preserved write-EPIPE regression. Renderers do not
receive this privileged API.

```js
import {spawn, execFile, exec, fork, ChildProcess} from 'node:child_process';
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

The shell option on spawn/execFile, IPC/additional stdio channels on spawn, stream-object
stdio adoption, AbortSignal, uid/gid and serialization options fail with
ERR_MITE_UNSUPPORTED_API. The separate exec API below now runs a genuine shell;
Synchronous process APIs remain absent; fork has the separate JSON IPC contract below. Direct construction
with new ChildProcess is also explicitly unsupported; use spawn.

## Process and stream lifetime

The returned object is an EventEmitter with pid, spawnfile, spawnargs, exitCode,
signalCode, killed, connected (false for ordinary spawn), stdin/stdout/stderr and stdio. Piped stdin is
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
fail explicitly. The shell option on execFile, AbortSignal and privilege switching
remain unsupported. Use exec below for shell commands. Synchronous
variants remain unavailable; execFile/exec custom promisify integration is supported. Extra stdio/detached/argv0 behavior from spawn is not part of the
execFile contract; this API always creates the three capture/input pipes.

```js
const child = execFile('/path/to/tool', ['argument'], {maxBuffer: 65536},
  (error, stdout, stderr) => {
    if (error) console.error(error.message);
    else console.log(stdout);
  });
child.stdin.end();
```

## exec(command[, options][, callback])

Run the original command through a genuine OS shell and collect stdout/stderr using
the same bounded native process/pipe implementation as execFile. Shell parsing,
quotes, pipes, redirection and expansion belong to that OS shell; Mite does not
split or emulate shell commands. Direct spawn/execFile remain shell-free.

```js
import { exec } from 'node:child_process';
import { promisify } from 'node:util';

const pending = promisify(exec)('printf "%s" "Grüße 🦋"');
const { stdout, stderr } = await pending;
console.log(stdout, stderr, pending.child.pid);
```

The command must be a nonempty string without NUL. Invalid/empty commands fail
synchronously, matching the measured Node 24 baseline. exec(command, callback) and
exec(command, options, callback) return the actual ChildProcess immediately.
Callback/output/error/close semantics, encoding, maxBuffer, timeout, killSignal,
cwd/env and capture limits follow execFile above. The error cmd is the original
shell command. Omitting a callback still executes and enforces collection limits.

A nonempty string shell option selects that shell. An empty string selects direct
execution of the entire command as the executable name (no token splitting). Otherwise POSIX uses /bin/sh with -c;
Windows source selects ComSpec/cmd.exe, using /d /s /c and native verbatim quoting
for cmd.exe, or -c for another shell. A false/nonstring shell option still selects
the exec default shell. Mite requires an object options argument and an object
environment; Node's broader primitive/array coercions are not supplied.
The Windows code path is implemented but native Windows shell acceptance is not
established. Environment replacement and working-directory behavior remain actual
native process behavior. No privilege switching, AbortSignal, synchronous APIs,
PTY, process-tree supervision or remote shell is supplied by this slice.

Local macOS differential tests and owned policy checks pass. The actual embedded
LibJS collector also runs during real delayed shell output and timeout; capture
callbacks and complete stdout/stderr survive after application child references
are dropped. This POSIX GC fixture does not accept Windows process replacement. The unchanged compiled
Code-OSS terminal-encoding consumer also passes under actual Node/Mite: its real
locale charmap command produces raw UTF-8 output with VSCODE_CLI_ENCODING unset,
then normalizes it through the original compiled platform/consumer dependencies.
This is neither terminal/PTY nor process-tree/workbench acceptance. Fresh native
Linux/Windows exec execution remains unverified; prior spawn/execFile platform
results do not promote this new shell path.

Both exec and execFile have custom util.promisify implementations. Their returned
Promise exposes the actual .child immediately, resolves with { stdout, stderr },
and attaches stdout/stderr to rejection errors. The callback path still owns all
native work and output/error policy. Existing execFile platform evidence below
remains distinct from the new local-only exec gate.

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

## fork(modulePath[, args][, options])

Locally accepted on macOS with real Mite children and Node 24 differential tests.
Fresh Windows/Linux fork acceptance remains unverified. This launches the current
Mite runtime with a separate LibJS VM and an inherited duplex IPC channel, without
Node/V8 or Firefly desktop capabilities in the child. It returns a ChildProcess.

```js
import { fork } from 'node:child_process';

const child = fork(new URL('./worker.mjs', import.meta.url), { silent: true });
child.on('message', (reply) => console.log(reply));
child.on('error', (error) => console.error(error));
child.send({ kind: 'job', value: 7 }, (error) => {
  if (error) console.error(error);
});
```

The worker installs `process.on('message', callback)` and responds through
`process.send(value, callback?)`. See the runnable modern ESM
[job example](../../examples/mite-fork/README.md).

| Option | Supported subset |
| --- | --- |
| modulePath / args | Path string or file URL; optional argument array. Child argv omits the internal bootstrap flag. |
| cwd / env | Existing spawn snapshots; one private channel marker is added and removed before child entry. |
| silent | false by default: inherit stdin/stdout/stderr. true creates three ordinary pipes. |
| stdio | pipe/ignore/inherit string or four entries with the sole ipc entry at index3. First three slots follow spawn. |
| execPath | Omitted or exactly current process.execPath. Foreign runtime executables are unsupported. |
| execArgv | Omitted or empty array. Parent eval metadata is not inherited. Engine/inspector flags are unsupported. |
| serialization | Omitted or json. Advanced serialization and handle/socket transfer are unsupported. |
| detached / windowsHide / windowsVerbatimArguments / timeout / killSignal | Existing spawn subset; platform acceptance remains separately scoped. |

The fourth descriptor is private to fork; ordinary spawn still rejects IPC and
additional descriptors. IPC uses JSON snapshots and UTF-8 newline framing, not
shared memory or structured clone. Dates/Buffers follow JSON serialization;
cycles/BigInt and unsupported top-level values throw. Snapshot getters can reenter
or disconnect: no stale write is admitted afterward.

## Fork IPC and lifetime

`child.send` / `process.send` accept a message and optional completion callback.
A true return means admission below the pressure threshold; false can mean an
accepted backpressured write or a rejected/closed channel. Completion callbacks
acknowledge native write completion, not application receipt. Error without a
callback emits error asynchronously. Closed sends report ERR_IPC_CHANNEL_CLOSED;
repeated disconnect reports ERR_IPC_DISCONNECTED. Validation and resource limits
have explicit codes; error wording/classes are not blanket Node parity claims.

Wire frames are bounded to 1 MiB including newline; at most 64 writes and 4 MiB are
pending per endpoint, with false returned at 128 KiB pressure. Receive data is
validated for UTF-8/JSON and partial-frame size. At most 64 messages/4MiB may wait
for a listener; the nextTick replay preserves FIFO. Reserved NODE_ commands route
to internalMessage. Malformed/overflow data closes with an explicit error.

`connected` becomes false when disconnect begins. Disconnect drains accepted local
writes and closes IPC; it cannot guarantee that the peer consumed a completed
write. EOF and native completion retire callbacks. The channel exposes ref/unref;
explicit choices override automatic holds. Parent IPC is referenced by default.
Child IPC starts unreferenced and ordinary message/disconnect listeners or pending
writes hold it unless explicitly unreferenced. Queued data alone does not keep a
listener-free child alive. Process-handle ref/unref remains distinct.

Spawn failures retain a closing IPC facade: early sends receive native EPIPE,
then closed sends receive CHANNEL_CLOSED. Independent children are not killed
merely because the parent VM exits. Teardown detaches JS callback roots before
native pipe/write cancellation; pending native ownership drains without reentering
a retired VM. This differs from Firefly utilityProcess's owned-child kill policy.

In a Firefly parent, the same executable enters a runtime-only role before Qt,
compositor or desktop-module initialization. The child has neither firefly imports
nor process.parentPort; Node process IPC is a separate protocol from utility
parentPort. The non-GUI fixture uses the production application object code and passes with a
deliberately invalid Qt plugin in the child, actual JSON exchange and child exit/reaping.
Packaged/platform acceptance remains separate.

## Fork validation and current Code-OSS scope

The [real fork runner](../../tooling/tests/test_fork.py) compares actual Node/Mite
self-children: typed/base64 snapshots, FIFO, 768 KiB pressure, late listeners, errors,
failed spawn, disconnect/EOF/exit, reference behavior and independent child survival.
Controlled channel tests are labeled separately. Private bootstrap checks reject
missing/wrong inherited capabilities; an environment marker alone exposes no IPC.

The pinned unchanged Code-OSS ipc.cp Client/Server passes 512 KiB VSBuffer/base64,
concurrent requests, remote errors, queued sender, events and natural child exit,
with 44 compiled dependencies/NLS integrity checked. Source pin/license is retained.
Run the existing prepared tree component with:

```sh
node tooling/code_oss/probe-fork.mjs build/code-oss-nls-final build/release-mite/bin/mite
```

This does not run the full watcher service or extension host. The actual full
local-desktop/NLS ready graph now passes fork and next fails linking Electron net,
imported by the suspended-path watcher. See [current milestones](../compatibility/vscode.md)
and the [synchronous exit contract](process.md#exit-notification).
