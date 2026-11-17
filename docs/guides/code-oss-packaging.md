# Package the partial Code-OSS sample

This is an experimental macOS ARM64 sample pipeline, not a full Electron or VS Code
release. The application runs on Firefly/Mite with LibWeb/LibJS. Node, pnpm and
esbuild are build tools only; there is no packaged Node/V8/Electron engine fallback.

The current Ladybird candidate `59158f57c` passes the standalone CLI suite.
The sealed sample built locally and passed the controlled CommonJS extension,
editor save, terminal and parent-liveness checks with the preceding macOS SDK.
The relocated signed bundle passed the same workflow through the native Cocoa
Launcher with that SDK. Current SDK and four-product release CI remain pending.

The [Code-OSS compatibility record](../compatibility/vscode.md) distinguishes earlier
controlled editor/terminal workflows, a visible macOS Welcome workbench and the
remaining general input/extension/startup limitations.

## Inputs and build

Build the native runtime/framework and install the repository's locked build tools
as described in [getting started](../getting-started.md) and the
[release guide](../development/releases.md). Keep one current source/dependency tree
and one current output instead of retaining a full copy for every attempt.

```sh
python3 tooling/build_code_oss.py \
  --source build/code-oss-git-source \
  --dependencies build/code-oss-dependencies \
  --overlay build/code-oss-release-overlay \
  --output build/code-oss-app
```

Select a fresh source location or an existing **clean Git checkout whose own root
is that exact path**. An extracted archive nested inside another repository is not
a Git source input. The builder checks the exact official remote, fetches the
recorded full commit and checks it out detached. It refuses other source changes;
Finder `.DS_Store` metadata is excluded from this source-dirty check and preserved.
It does not fetch or check out a parent repository discovered by `git -C` traversal.

The revision, official archive hash and static-extension inventory are pinned in
[the builtin-extension manifest](../../tooling/code_oss/builtin-extensions-pin.json).
The matching source archive is independently size/hash checked before static
extensions are staged. The selected compiler/adapter/dependency inputs also have
pinned-source checks. A verified existing dependency tree may be reused only for
the matching source, installed state, platform and architecture. Dependency
lifecycle scripts remain disabled.

Output and overlay must be fresh, separate trees. Existing outputs are refused;
this command does not silently refresh or merge a release stage. Remove only
superseded **owned** outputs after their native processes have stopped, preserving
source changes, registered worktrees, accepted packages and Finder metadata.
Do not delete an existing archive-source tree just to make it look like the required
Git checkout; choose the intended input explicitly.

The compiler stages real JavaScript, renderer assets, CSS discovery inventory and
NLS data, along with the selected native compatibility package adapters. The
maintained `main.mjs` selects the source-derived development renderer using
`VSCODE_DEV=1` and the explicit local-desktop policy. It contains no offscreen,
diagnostic or test-extension flags. The limited Electron import alias is supplied
by the native package Launcher, not by a bundled Electron engine.

## Seal and relocation

`BUILD.json` identifies the exact upstream revision, compilation metadata hash,
sealed inventory hash and partial static-extension counts. It explicitly records
`desktopAcceptance: 'not-established-by-build'`.
`ELYTRA-SAMPLE-FILES.json` inventories physical payload byte hashes, lengths,
executable-bit records and relative link targets. The maintained entry and
required renderer/NLS compile modes are checked separately.

`BUILD.json`, the inventory, `ELYTRA-COMPILE.json`, `package.json` and `main.mjs`
must be physical regular files. Internal or external symlink aliases for these
authorities are rejected before they are read. Dependency aliases inside the tree,
including pnpm links and `out → src`, remain genuine relative links. Absolute,
dangling or escaping links, payload changes, missing files and unrecorded additions
are rejected. Finder metadata is excluded from the seal.

Executable-bit inventory checks describe the native mode reported by the host.
Windows Python `chmod` does not provide POSIX execute-bit semantics; a source-only
Windows check is not execute-ACL, PE, desktop or package acceptance.

```sh
python3 tooling/package_macos.py \
  --mite-build build/release-mite --renderer-build build/renderer \
  --designer build/designer-app --code-oss build/code-oss-app \
  --output build/release-stage --version 0.1.0 \
  --dependencies production --symbols stripped
```

The packager verifies the sample before copying and again inside
`Code-OSS.app/Contents/Resources/app`. It preserves internal relative links and
physical runtime resources, stages the actual helper/library closure and checks
the limited Electron-alias build policy. Existing native dependency, entitlement,
RPATH, signing and bundle-size checks still apply. These checks cannot be replaced
by the JavaScript seal. The package is ad-hoc signed, not notarized; older-host,
installer and update support remain unverified.

## Open the macOS preview

Use an isolated profile with the native title bar. On the verified preview
bundle, the default profile requests the unsupported `titleBarOverlay` option.
Run the following from the package directory, or set `app` to the absolute path
of your unpacked bundle.

```sh
app="$PWD/Code-OSS.app"
state="$(mktemp -d "${TMPDIR:-/tmp}/elytra-code-oss.XXXXXXXX")"
mkdir -p "$state/profile/User" "$state/extensions" "$state/xdg"
printf '%s\n' '{"window.titleBarStyle":"native","workbench.startupEditor":"welcomePage"}' > "$state/profile/User/settings.json"
(
  cd "$app/Contents/Resources/app" || exit
  QT_QPA_PLATFORM=cocoa XDG_CONFIG_HOME="$state/xdg" \
    XDG_DATA_HOME="$state/xdg" XDG_CACHE_HOME="$state/xdg" \
    "$app/Contents/MacOS/Launcher" \
    --user-data-dir "$state/profile" --extensions-dir "$state/extensions" \
    --new-window --disable-crash-reporter
)
```

The preceding macOS SDK displayed the Welcome workbench through this launch
path. The profile and extensions directory keep the preview independent of your
personal settings. The runtime checks that the profile directory already exists.

## Required native workflow

The release driver first requires the ordinary development-extension CJS workflow
against the source stage and actual offscreen frontend, using `--sample-entry`.
The original application performs extension activation, commands, document
open/show/edit/save, real parent-liveness checks and real terminal creation.
The terminal gate authenticates the owned Bash PID before `sendText`, then requires
exit 17 and independent physical output readback. A successful API Promise alone
is insufficient evidence.

After packaging and the existing native audits, the driver copies **one** signed
Code-OSS.app to an owned temporary location. It then uses the real packaged
`Contents/MacOS/Launcher` and the Cocoa provider. The fixture passes only normal
Code-OSS arguments; the Launcher supplies its own recorded `--electron-compat`
and sealed application entry. No alternate test entry is substituted.

For an explicitly selected native acceptance run, the same fixture is:

```sh
python3 tooling/tests/test_code_oss_workbench.py \
  --bundle '/relocated/Code-OSS.app' \
  --extension-format commonjs --terminal --parent-liveness \
  --state-directory /tmp/ef-release-codeoss
```

This is a real visible Cocoa acceptance, intended for the batched native CI gate
or deliberate renderer/package verification, not repeated documentation checks.
Strict deep code-signature and sample-seal checks precede launch. The main process
must be observed executing only the exact verified bundle Launcher/Firefly pair,
with matching native birth/UID/owned-tree identity. Existing descendant cleanup,
nonce-bound extension results, saved-document bytes and terminal checks remain
required. A different executable, timeout, missing result, failed physical readback
or unconfirmed cleanup fails the gate. Logs are redacted after processes stop;
failed ownership/cleanup retains private evidence instead of declaring success.

The temporary relocated copy is removed only after successful acceptance. Failure
retains the sole owned copy for investigation rather than deleting potentially
live executable/resources. Local Cocoa acceptance passed on the preceding
macOS SDK with all 16 observed processes stopping naturally. The current SDK
still needs to pass this gate in the release pipeline.

## Artifacts and remaining support

A successful current release build creates four product archives: `mite`,
`firefly`, `web-component-designer` and `code-oss`, plus SIZE and SHA256SUMS metadata.
The promotion verifier binds their unchanged bytes and matching build/source
records to one successful trusted native workflow, then checks uploaded draft
asset names, sizes, states and server SHA-256 digests before publication.
Historical Alpha 1/2 remain their original three-product/five-asset releases.

Code-OSS includes static builtin extensions only; unbuilt runtime extensions,
webviews, remote modes, native-addon ABI compatibility and engine code cache remain
unsupported or unaccepted. A Welcome workbench is not proof of all interactive
editing, menus, terminal load/failure behavior or arbitrary extensions.

Windows providers and AppContainer/broker component source do not establish a
complete Windows application or secure executable package. Full helper-role
coverage and Browser-origin broker retention/backpressure remain blockers; the
full Windows runtime stays gated. Existing Linux package experiments also retain
their recorded host/scope rather than inheriting acceptance from this macOS pipeline.
