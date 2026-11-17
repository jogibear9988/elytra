# mite:pty

Status: partial native pseudoterminal service. Standalone Mite passes the focused
headless macOS controls against Node 24.15 with unchanged node-pty 1.2.0-beta.18.
The same six groups and two policy/one shutdown groups also pass in rebuilt
Firefly-as-Node. A controlled Code-OSS terminal workflow without diagnostic flags now
passes on local macOS; general terminal reliability/UI acceptance remains open.
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
The scoped CA-31 policy then advances through the original PTY host. Earlier
exact-executable and timeout observations are recorded in
[ADR 0205](../architecture/0205-code-oss-pty-inspector-policy.md).

No terminal window or desktop interaction is used. These controls do not establish
Windows, fresh Linux, visible terminal rendering/input in Code-OSS, or
arbitrary process-tree cleanup.

## Controlled Code-OSS terminal result

A local macOS offscreen run with `FIREFLY_IPC_BYTES_DIAGNOSTIC=1` now passes the
original CommonJS extension activation/commands/open-edit-save workflow AND real
terminal creation. It obtains Bash PID 70351 through the original PTY host,
authenticates native birth/UID/exact executable/owned-tree identity, sends printf
through `Terminal.sendText`, observes terminal exit 17 and independently verifies
the physical terminal-output bytes in Python. Main exits 0; all 15 observed native
processes are independently absent and cleanup sends zero signals.

Terminal-output SHA-256:
`416a3fc023adf8f1ead2c9fd75ec57f6ce4dec1cdc271d6492593b3e61d491df`.
Editor-output SHA-256:
`4077386fa9fddf27feeab8e86fb0fc3645ee636c8c12d244c8b3dc4a621cfee6`.
Stage SHA-256:
`f695517d6ddf8bd8d6cdc3c94903beac84549ecfafba390b625eaba49b520573`.
Evidence: `build/codeoss-pty-admission-diagnostic-{acceptance.log,evidence.json,workbench.log}`.

A subsequent local macOS offscreen run **without the diagnostic flag** passes
both workflows after bounded queued attachment admission. It authenticates Bash
PID 74547; main exits 0, all 14 observed native processes are independently absent
and cleanup sends zero signals. Terminal SHA-256:
`ec081f8801c32b9b8ad3fb4d5f7cb9e796ffc415b07aeef46689cb55e152b163`;
editor SHA-256:
`02248aa5ab28eec5dfd835ef833e2c60422794f2e0630249b6bf85f1f74a410c`.
The stage digest is unchanged. Evidence:
`build/codeoss-queued-port-{acceptance.log,evidence.json,workbench.log}`.
The mixed renderer-port regression also passes full active-window handoff,
finite backlog refusal/recovery and old-window cancellation, without increasing
quotas. See [ADR 0206](../architecture/0206-queued-renderer-port-attachments.md).
Visible rendering/input, fresh platforms and broader load/failure coverage remain
unaccepted. The diagnostic checkpoint above retains its original run scope.

A subsequent native-stdio Release rebuild repeats the non-diagnostic real
CommonJS editor/terminal workflow successfully (Bash PID 79529, main exit 0).
All 15 observed native process identities are independently absent after two
helper cleanup signals. This run proves the controlled workflow, not natural
helper teardown. Terminal SHA-256:
`7d1b1876f9ac9f207a0d4bc39c7d4df2ddd0a3b6c7624108fea0673046b09deb`;
editor SHA-256:
`4243d6c29145a57c85b19a5c54c23668eda606a3fa81d5263e5f18f707912ab1`.
The reused stage digest is unchanged. Evidence:
`build/codeoss-native-stdio-{acceptance.log,evidence.json,workbench.log}`.

The latest native-process-signals rebuild adds a longer non-diagnostic controlled
run: three 1100 ms extension continuations each check the real native parent with
signal zero, execute the original registered command and verify the saved editor.
The real Bash terminal then passes sendText/exit17 (PID 82036). Main exits 0; all
15 observed native identities are independently absent, with zero cleanup signals.
This establishes this timed continuation, not every internal timer or general
long-running stability. Terminal SHA-256:
`41571b2d5f2ed43ec6e725f556504b47c377cbaad032eadab8ec76abd929ac75`;
editor SHA-256:
`9513eef516fa89e320f647344bbe55cdaa44ac0c06acc9aa2a8edbe123648640`.
Stage digest unchanged. Evidence:
`build/codeoss-parent-liveness-{acceptance.log,evidence.json,workbench.log}`.

After the physical control-reserve rebuild, the same non-diagnostic parent-liveness,
editor and real terminal workflow passes again (Bash PID 86056). Main exits 0;
all 14 observed native identities are independently absent, with zero cleanup
signals. The separate native pressure probe and actual renderer-port/binding
batches also pass; this is scoped regression evidence, not arbitrary-load or
visible-desktop acceptance. Terminal SHA-256:
`57a0ce3411d9298d58ae715d412128e55c46a6e3696574a5841d3dab45638f24`;
editor SHA-256:
`46090729b4b8ec4087b82d2510df7e233dd54d59fa9f88f159c900545c2588a0`.
Stage digest unchanged. Evidence:
`build/codeoss-physical-control-{acceptance.log,evidence.json,workbench.log}`.
