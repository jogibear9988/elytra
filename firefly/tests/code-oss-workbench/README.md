# Actual CodeOSS workbench acceptance

This ordinary ESM development extension and extension-test runner use the real
`vscode` API. They require successful extension startup, register/invoke a command,
open an owned file in a real editor, apply a WorkspaceEdit and save. The parent
runner independently verifies the saved bytes and the nonce-bound result.

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
controlled activation/open/edit/save workflow, not terminal, search, extension
marketplace, external network or general CodeOSS compatibility. This fixture is
prepared, not yet an accepted application run. The current local extension-host
policy explicitly rejects development/test mode before host startup. No ordinary
CLI debug option bypasses that admission: in this development tree,
`--inspect-extensions=0` still selects the implicit debug port. The runner does
not weaken that policy or claim activation acceptance while it remains blocked.
