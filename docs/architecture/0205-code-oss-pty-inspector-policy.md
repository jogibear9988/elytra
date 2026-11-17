# ADR 0205: Explicit local omission of the implicit PTY-host inspector

Status: implemented source adaptation CA-31; 32 focused Node/Mite cases and
existing source/local-desktop/shared/engine policy controls pass locally. Actual
Code-OSS now passes a controlled diagnostic-flagged native terminal workflow;
general terminal reliability and visible UI acceptance remain open.
This is not an inspector backend.

## Observed blocker

The native PTY backend and exact node-pty package port pass standalone and embedded
controls. The first actual Code-OSS terminal fixture fails earlier: the original
ElectronPtyHostStarter uses the development parser's default inspector port 5877,
then supplies `--nolazy` and `--inspect=5877` to utilityProcess. Those engine flags
are genuinely unsupported by LibJS. That run times out without reaching the
fixture's PTY PID or terminal-byte gates; all 13 observed processes stop with no
cleanup signals. Native PTY acceptance does not make those flags valid.

## Decision

Adapt only the exact pinned `electronPtyHostStarter.ts` parser call through
`firefly/compat/code-oss/pty-host-debug.ts`. The owned transformer
`tooling/code_oss/pty_host_debug.py` verifies source SHA-256
`20776b27d13f59641b0d51407fa8e0b48874e36d6226876253e3f0c3d6963e47`,
requires unique import/call seams and reverses the output to the original bytes.
The pinned source and license remain retained; upstream Ladybird is unchanged.

Ordinary Node/Electron calls the original parser once and returns its exact
result. Only the explicit verified local Mite/LibJS desktop profile with disabled
engine cache can omit the development default. It requires isBuilt false and the
original result `port: 5877, break: false`, then returns a data copy with port null.
That selects the existing upstream no-inspector path instead of changing native
utility-process flag validation. Built/no-debug calls preserve the parser result
identity; parser exceptions propagate unchanged.

Explicit `inspect-ptyhost` or `inspect-brk-ptyhost` requests fail with
`ERR_CODE_OSS_LIBJS_ENGINE_OPTION`, including empty or false explicit values and
argv.json input. Unexpected parser results, accessors including symbol accessors,
engine/cache/local-profile mismatch and status collisions fail. The helper
revalidates local admission around the parser and descriptor snapshot.

The immutable `_vscodeLibJSPtyHostDebugStatus` record discloses
`available: false`, scope `code-oss-local-implicit-pty-host-inspector-only` and
omitted flags `--nolazy`, `--inspect=5877`. It is published only when omission
actually occurs. It never advertises a working debugger, V8 mode or an inspector
port that does not exist.

## Validation and removal

Focused 32-case Node/Mite and existing policy controls pass. The reused compiled
stage contains 12,348 source files, 32 overlay files, seven package adapters and
the same 60 staged static extensions. The first post-guard actual run reaches the original PTY host and a genuine shell
PID (64569), authenticated by native birth, UID and owned process-tree checks.
Its strict expected-executable gate correctly refuses `/bin/sh` versus actual
`/bin/bash`: on this macOS host `/bin/sh` is a distinct selector executable that
execs Bash. All 19 observed processes stop; cleanup sends 14 signals. This is not
a waiver of executable authentication or terminal acceptance.

The fixture then explicitly requests `/bin/bash --noprofile --norc -i`, retaining
exact executable/birth/UID/tree authority and avoiding user startup files. That
run creates PTY-host PID 65017 but never reaches the terminal-ready PID or byte
result; the extension's 30-second deadline expires. All 14 observed processes stop
with zero cleanup signals. The connection/runtime cause remains under
investigation; neither the guard nor the native backend is established as its
cause. Evidence: `build/codeoss-pty-host-debug-{acceptance.log,workbench.log}` and
`build/codeoss-pty-bash-{acceptance.log,workbench.log}`. These earlier failed attempts did not accept a terminal workflow; source controls
alone do not establish it.


A later actual diagnostic-flagged run passes original CJS editor operations plus
terminal creation, strict native Bash identity, sendText printf, exit17 and
independent physical byte verification. Main exits0, all15 observed processes are
absent and cleanupSignals0. No quotas changed. One separate 364-byte transfer is
still refused at the 16-active outgoing IPC window, so generic under-load
reliability remains open. This is controlled local macOS offscreen evidence, not
an inspector or visible terminal UI claim. See the
[exact hashes and evidence](../api/pty.md#controlled-code-oss-terminal-result).

CA-31 remains an open deferral. Remove the helper/import/manifest transformation
when a genuine inspector contract can execute the original flags, or an upstream
runtime-neutral disabled-inspector policy makes the adaptation unnecessary. Restore
the pinned starter bytes and test explicit debugging, development defaults, built
no-debug behavior, child execArgv and actual terminal launch. Preserve Node/Electron
and malformed-policy controls until then. See the
[adaptation tracker](../development/code-oss-adaptations.md) and
[native PTY contract](../api/pty.md).
