# ADR 0204: Native pseudoterminals without Node or post-fork VM code

Status: implemented partial POSIX backend; focused standalone macOS differential,
native lifecycle/GC and shutdown controls pass standalone and in rebuilt Firefly
embedding. A controlled diagnostic-flagged Code-OSS terminal workflow passes;
fresh Linux and general terminal/UI acceptance remain pending.
Windows is unsupported.

## Demand

Code-OSS uses node-pty to create a controlling terminal, resize it, exchange bytes,
apply read backpressure and observe real child exits. Pipes or an import-only
node-pty class cannot provide those semantics. The pinned package's original
native-addon/Node stream implementation cannot execute directly inside Mite.
Mite must remain independent of Firefly, LibWeb, Node and V8.

## Decision

Provide the owned `mite:pty` module over a native master/slave PTY. The facade
preserves the exercised node-pty terminal API; the opt-in package adapter selects
it only under Mite at the exact node-pty 1.2.0-beta.18 entry. Preserve the original
Node/Electron branch bytes and MIT notices. Unsupported options reject explicitly.

Spawn a fresh image of the same executable with a private early
`--mite-pty-child-v1` mode, before any VM or Firefly application initialization.
The parent allocates/configures the terminal and snapshots argv/environment. Spawn
file actions supply the slave as descriptors 0–2 and a startup status pipe as 3;
owned descriptors are relocated above them to avoid low-FD collisions. macOS uses
CLOEXEC-default spawn behavior; the Linux path uses glibc's closefrom file action.

The fresh helper creates a session with setsid, reopens the slave and establishes
the controlling terminal, changes cwd, restores signal mask/defaults and execs
the real target with the supplied environment. It marks status FD 3 CLOEXEC before
target execution. The parent accepts EOF as successful exec or a fixed errno
record as failure, with a five-second poll deadline. No LibJS/Qt callbacks or
application C++ execute in a child forked from a live VM.

This helper is necessary for portable ordering: Apple's spawn file actions run
before its SETSID action, whereas glibc performs setsid before file actions. Simply
opening the slave in a spawn file action cannot establish the same macOS contract.
The helper is part of the existing executable, not an extra package binary.

## Native ownership

The runtime owner keeps at most 16 terminal records. Each owns a nonblocking
master, a uv_poll handle, a referenced reaper timer, rooted callback/context and
up to 1 MiB of queued byte snapshots. Poll callbacks read at most 64 KiB or perform
bounded partial writes. Pause stops reads while the reaper remains active.

Reaping uses waitpid for the exact owned PID with WNOHANG; it neither replaces
libuv's SIGCHLD machinery nor calls waitpid(-1). Installed libuv 1.52.1 also reaps
its own registered process handles by exact PID. An observed reaped PID is
invalidated before callbacks to prevent later signals reaching a reused PID.
EOF and reaping jointly control the final exit, with the pinned node-pty 200 ms
post-reap drain grace. Native handle-close callbacks retain terminal storage until
both handles are closed. No GC value crosses threads.

Runtime shutdown first disables callbacks and releases JS roots, then closes
masters, kills/reaps owned direct children and drains native handle closure.
Explicit destroy closes the master and requests SIGHUP, escalating after one
second. These are owned-shell semantics, not a guarantee for arbitrary descendants
that change sessions or process groups. Foreground process metadata uses the
actual terminal process group and native OS query, with empty result on failure.

## Acceptance and limits

The [PTY API](../api/pty.md) records exact options, errors, bounds and test command.
Six shared unchanged-node-pty/Node 24.15 and Mite groups pass locally on macOS,
including real controlling-terminal identity, UTF-8/data ordering, resize signal,
paused output, drain and signals. Independent Python checks verify observed child
exit. Two Mite policy groups verify binary and real GC ownership, and a separate
runtime-shutdown group passes. Rebuilt Firefly-as-Node repeats all six shared, two
policy and one shutdown groups. The adapted package entry passes the unchanged
Node six groups and Mite shared/policy/shutdown groups. No local GUI is launched.

The current Linux implementation requires glibc 2.34 or newer; it is not fresh
platform acceptance. Windows needs a real ConPTY provider with output-drain-aware
closure, process handles and native lifetime tests; WSL and pipe emulation are not
substitutes. General node:tty streams, privilege changes, arbitrary encodings,
arbitrary native addons and full process-tree cleanup are outside this slice.
The first actual Code-OSS terminal fixture reached the original ElectronPtyHostStarter
but fails its unsupported utility-process inspector flag request before any PTY
PID/output gate. It times out; all 13 observed processes stop without cleanup
signals. That historical failure preceded CA-31; this native backend slice does not relax
utility-process policy. A later controlled diagnostic-flagged run now passes real
terminal creation, native shell identity, sendText, exit17 and independent physical
output verification alongside the original CJS editor workflow. Main exits0; all15
observed processes are absent without cleanup signals. A separate active-window
port refusal remains, so reliability under load is not accepted. See the
[exact current evidence](../api/pty.md#controlled-code-oss-terminal-result).

## Primary source basis

- Pinned node-pty 1.2.0-beta.18 `src/unix/pty.cc`,
  `src/unix/spawn-helper.cc` and `lib/unixTerminal.js`, including its 200 ms drain
  timeout and MIT license.
- [Apple spawn implementation](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_exec.c)
  and [glibc spawn implementation](https://github.com/bminor/glibc/blob/master/sysdeps/unix/sysv/linux/spawni.c)
  for file-action/session ordering.
- [libuv 1.52.1 process implementation](https://github.com/libuv/libuv/blob/v1.52.1/src/unix/process.c)
  for exact-PID child reaping.
- [Microsoft pseudoconsole lifecycle](https://learn.microsoft.com/en-us/windows/console/closepseudoconsole)
  for the distinct Windows output-drain and closure requirements.
