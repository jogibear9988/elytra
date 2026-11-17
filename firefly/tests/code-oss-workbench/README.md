# Actual CodeOSS workbench acceptance

This ordinary ESM development extension and extension-test runner use the real
`vscode` API. They require successful extension startup, register/invoke a command,
open an owned file in a real editor, apply a WorkspaceEdit and save. The parent
runner independently verifies the saved bytes and the nonce-bound result.

The default extension stays ESM. `--extension-format commonjs` selects the
explicit `.cjs` compatibility fixture using the same original extension scanner
and ESM test runner. Its entry and nested helper repeatedly require `vscode`
through CodeOSS's original `Module._load` interceptor, verify API identity, and
run the same actual command/open/edit/save checks. The parent authenticates the
requested format in the current result; a previous ESM result cannot pass a
CommonJS run. Both modes reuse the same owned profile and stage.

Run as an offscreen acceptance batch using the existing prepared stage and
current diagnostic binary:

```sh
python3 tooling/tests/test_code_oss_workbench.py \
  --firefly build/renderer-review/bin/firefly-view-offscreen \
  --stage /absolute/path/to/code-oss-window-cache-final \
  --state-directory /tmp/ef-workbench-acceptance --timeout 90
```

The runner passes ordinary `--extensionDevelopmentPath` / `--extensionTestsPath`
arguments directly to the original staged entry. It does not patch source,
install dependencies, clone the stage, simulate editor APIs or inject UI input.
It sets the existing documented local-desktop/offscreen development environment
and uses a marker-owned reusable short profile, preserving Finder metadata.
It passes the ordinary `--disable-crash-reporter` option used by the accepted
startup diagnostic baseline, so CodeOSS does not request the currently unsupported
OS crash-handler suppression mode. This is a CLI choice, not an API bypass or a
change to the application's crash-reporter source.

Output logs are bounded and sanitized only after all authenticated descendants
have stopped. Descendants are tracked by native ancestry, executable and precise
birth identity; cleanup includes children in separate process groups. Windows
process ownership is not implemented by this runner and fails explicitly.

A result file without correct native saved bytes is a failure. A timeout, test
failure or nonzero application exit is a failure. Even success proves only this
selected workflow. Editor-only success does not establish the optional terminal
workflow, search, marketplace, external network or general CodeOSS compatibility.

Current local macOS result (2026-11-17): CommonJS/editor, timed parent-liveness
continuations and the real terminal pass together without diagnostic flags.
Three 1100 ms continuations each require native parent existence, a fresh original
extension command reply and a coherent saved editor. The application exits 0;
all 14 observed native identities are independently absent with zero cleanup
signals. Evidence:
`build/codeoss-physical-control-{evidence.json,workbench.log,acceptance.log}`.

This is controlled offscreen API acceptance, not visible painting/keyboard input,
all extensions, shell integration, another platform or general long-running
stability. Queued attachment admission now passes the mixed native regression
within unchanged quotas; broader saturation/failure coverage remains open.

Earlier editor-only ESM/CommonJS evidence is retained as
`build/codeoss-commonjs-{evidence.json,workbench.log,acceptance.log}` and
`build/codeoss-commonjs-esm-{evidence.json,workbench.log,control.log}`. Those runs
used separate binaries and required two helper-cleanup signals; their executable
hashes and teardown scope remain distinct from the current terminal checkpoint.

Add `--terminal` to opt into a separate real terminal workflow in the same
application batch. The default editor-only workflow remains unchanged. For
example, append `--terminal --timeout 120` to the command above; it also works
with `--extension-format commonjs` while the test driver remains ESM.

The extension explicitly creates `/bin/bash --noprofile --norc -i` with
startup/history environment paths set to
`/dev/null`, an owned workspace cwd and `isTransient:true`. Before sending any
commands, it records its positive shell PID with the run nonce. The Python
parent requires that live PID to belong to the authenticated native descendant
tree, verifies its birth/UID and actual `/bin/bash` executable, then atomically
publishes a nonce/PID gate file. This is test ownership coordination, not a
production permission mechanism or a replacement terminal API.
The exact Bash executable is requested deliberately: on macOS `/bin/sh` can be a
selector that execs `/bin/bash`, so checking the requested `sh` path against the
live executable would be incorrect. The fixture does not allow arbitrary exec
transitions; it requires the exact resolved Bash executable and disables user
profile/RC loading.

Actual `Terminal.sendText` runs a quoted `printf` into an owned file and exits
17. Acceptance requires the matching `onDidCloseTerminal` event and
`exitStatus.code === 17`, current authenticated result data, and independent
Python verification of the exact physical bytes after application cleanup. A
wrong nonce, foreign/dead PID, missing output, timeout or nonzero application
exit fails. `report.json` includes the terminal flag, native identity and output
hash only for this mode. The accepted terminal checkpoint above covers these
checks; prior failures and outstanding admission reliability are recorded in
`docs/PROGRESS.md`.

Add `--parent-liveness` to keep the actual extension alive across three 1100 ms
continuations. Each checks `process.kill(process.ppid, 0)`, executes the original
registered command and verifies the saved editor. Original Code-OSS separately
polls its main-process PID once per second; this mode tests continued API use
beyond that interval, without patching its watchdog or fabricating liveness.
The parent requires the authenticated current result's parentLiveness flag; the
report includes it only when requested. It can run together with `--terminal`.
