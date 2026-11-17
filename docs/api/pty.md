# mite:pty

Status: partial native pseudoterminal service. Standalone Mite passes the focused
headless macOS controls against Node 24.15 with unchanged node-pty 1.2.0-beta.18.
The same six groups and two policy/one shutdown groups also pass in rebuilt
Firefly-as-Node. An actual Code-OSS terminal workflow is not yet accepted.
Linux requires glibc 2.34 or newer for the current descriptor-closing spawn path;
fresh Linux acceptance is pending. Windows has an explicit unsupported provider.

```js
import { spawn } from 'mite:pty';

const terminal = spawn('/bin/sh', [], {
  name: 'xterm-256color',
  cols: 80,
  rows: 24,
  cwd: process.cwd(),
  env: process.env
});
const output = terminal.onData(data => process.stdout.write(data));
terminal.onExit(({ exitCode, signal }) => {
  output.dispose();
  console.log({ exitCode, signal });
});
terminal.write('printf "hello\\n"\n');
```

## Terminal surface

`spawn(file, args = [], options = {})` returns an EventEmitter backed by a real
OS PTY master/slave pair. `fork` and `createTerminal` alias `spawn`. This is not
ordinary child-process pipes, a replacement for `node:tty.ReadStream`, or a
Node native-addon ABI.

| Member | Behavior |
| --- | --- |
| `pid`, `cols`, `rows` | Actual spawned child PID and successfully applied terminal dimensions. |
| `process` | Native foreground process-group leader query; empty when unavailable or closed. Complete process-title parity is not established. |
| `onData(listener)` | Data subscription returning `{ dispose() }`; UTF-8 text by default, Buffer with `encoding: null`. UTF-8 decoding retains split sequences. |
| `onExit(listener)` | Subscription receiving `{ exitCode, signal }` after native reaping and output closure/drain grace. |
| `write(data)` | Accepts string, Buffer or Uint8Array; snapshots bytes into a bounded native queue. Acceptance is not proof the shell consumed them. |
| `resize(cols, rows, pixelSize?)` | Applies native TIOCSWINSZ, including optional `{ width, height }`; updates stored dimensions only after success. |
| `pause()`, `resume()` | Stops/restarts master reads, preserving kernel backpressure and child reaping. |
| `kill(signal = 'SIGHUP')` | Signals the owned unreaped child PID using the supported signal allowlist. |
| `destroy()` | Closes the master, requests SIGHUP and escalates to SIGKILL after one second if the child remains alive. |

The EventEmitter also emits `data`, `error`, `close`, and `exit(code, signal)`.
Supported signals are SIGHUP, SIGTERM, SIGKILL, SIGINT, SIGQUIT, SIGUSR1, SIGUSR2,
SIGCONT and SIGSTOP.
`close` precedes `exit`; no data is delivered after the final exit. A reaped child
gets up to 200 ms for final master reads before closure, matching the pinned Unix
node-pty timeout. Paused or inherited-slave output can be discarded at that bound.
The native reaper remains referenced even when reads are paused.

`clear()` has the Unix node-pty no-op behavior; it does not claim to clear a
ConPTY display cache. `setParentWindow()` returns false. Raw `open()` and native
addon access fail with `ERR_MITE_UNSUPPORTED_API`.

## Options and bounds

Supported options are `name`, `cwd`, `env`, `cols`, `rows`, `encoding`,
`handleFlowControl`, `flowControlPause` and `flowControlResume`. Defaults are
80 columns, 24 rows, current cwd/environment, UTF-8, and flow control disabled.
TERM uses `name`, then the supplied TERM, then `xterm`; PWD is set to the selected
cwd. With flow control enabled, writes exactly matching the pause/resume strings
change read state instead of reaching the shell; defaults are XOFF/XON.

Arguments and environment values must be strings without NUL. The child receives
an owned environment snapshot, and executable PATH lookup happens in that child
after changing cwd. Privilege switching (`uid`/`gid` other than omitted or -1),
enabled ConPTY options, other encodings and unknown options fail explicitly.

- At most 16 native terminals per runtime owner, including terminals awaiting
  native handle closure.
- At most 1 MiB queued write bytes per terminal; reads use chunks up to 64 KiB.
- At most 1,024 arguments and 512 final environment entries; the facade reserves
  two environment slots for PWD and TERM.
- Each string is bounded to 64 KiB, and argv/cwd/environment have a combined
  1 MiB byte budget. Dimensions are 1..65,535; pixel dimensions are 0..65,535.
- Fresh-child startup has a five-second native handshake deadline.

Invalid facade arguments use `ERR_INVALID_ARG_TYPE`, `ERR_INVALID_ARG_VALUE`,
`ERR_OUT_OF_RANGE` or `ERR_UNKNOWN_SIGNAL`; facade budget failures use
`ERR_MITE_PTY_LIMIT`. Native OS/queue failures retain libuv-style code/syscall
metadata, including ENOBUFS. Writes after closure use `ERR_MITE_PTY_CLOSED`.

## Ownership and package integration

Native callbacks retain the active terminal until native exit/closure. Destroying
the JS wrapper's last ordinary reference does not abandon its live child. Runtime
shutdown revokes callbacks, closes master descriptors and kills/reaps owned direct
children before destroying native handles. Closing a controlling terminal can
hang up its foreground group, but this is not a guarantee that every detached
descendant of an arbitrary shell has stopped.

The opt-in Code-OSS package preparation flag `--mite-pty` selects `mite:pty` only
under Mite at the exact pinned node-pty entry. Original Node/Electron code bytes,
package identity and license notices are retained. This adapter does not implement
N-API or make arbitrary node-pty versions compatible. See
[ADR 0204](../architecture/0204-native-pseudoterminals.md).

## Validation

The focused runner is `tooling/tests/test_pty.py --mite <mite> --node-pty
<pinned-package-directory>`. Six shared Node/Mite groups pass on local macOS:
actual controlling terminal/session/cwd/environment, UTF-8 FIFO/echo, resize and
SIGWINCH, paused 128 × 4 KiB output, exit/output drain, Ctrl-C/SIGINT and SIGTERM.
Independent Python OS checks verify child disappearance while the main process
remains alive. Two Mite policy groups cover binary data and active-wrapper
GC/reclamation/destroy, and a separate shutdown group verifies direct-child cleanup.
The rebuilt SDK/Firefly build passes, and Firefly-as-Node passes the same six
groups plus policy/shutdown controls. The conditional package entry also passes
six unchanged Node groups and the Mite six groups plus policy/shutdown controls.
Evidence: `build/pty-native-controls.log`, `build/pty-mite-build.log`,
`build/pty-firefly-build.log`, `build/pty-embedded-controls.log`,
`build/pty-adapted-node-controls.log` and `build/pty-adapted-mite-controls.log`.

The refreshed Code-OSS stage includes the seventh package adapter. Its actual
terminal fixture fails before obtaining a PTY PID or terminal output: the original
ElectronPtyHostStarter requests unsupported utility-process inspector flags.
The fixture times out; all 13 observed processes stop with zero cleanup signals.
Evidence: `build/codeoss-pty-acceptance.log` and `build/codeoss-pty-workbench.log`.
This records a separate launch-policy blocker, not accepted Code-OSS terminal use.
The subsequent scoped CA-31 policy advances through the original PTY host, but
actual terminal PID/output acceptance remains open; the latest exact-executable
and timeout observations are recorded in
[ADR 0205](../architecture/0205-code-oss-pty-inspector-policy.md).

No terminal window or desktop interaction is used. These controls do not establish
Windows, fresh Linux, terminal rendering/input in Code-OSS, or
arbitrary process-tree cleanup.
