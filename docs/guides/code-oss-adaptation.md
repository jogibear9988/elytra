# Code-OSS adaptations for LibJS

Code-OSS desktop is not yet supported. This guide records deliberate application
changes needed to run its pinned source on Firefly/Mite without pretending to be
Node/V8. The [blocker inventory](../compatibility/vscode.md) defines the broader
target and remaining gates.

## Prepare the native process identity overlay

Code-OSS commit 6531648f5ace8b77ae799262bdc97bf048461938 uses versions.node to
recognize native unsandboxed execution. Mite uses versions.mite. The overlay adds
the latter to the two checks and their TypeScript interface; all other source
bytes remain unchanged. It keeps Node's existing path intact and does not classify
Mite as Electron or expose privileged process data to a renderer.

```sh
git clone --filter=blob:none --no-checkout https://github.com/microsoft/vscode.git build/code-oss-source
git -C build/code-oss-source -c core.autocrlf=false checkout 6531648f5ace8b77ae799262bdc97bf048461938
python3 tooling/code_oss/overlay.py --source build/code-oss-source --output build/code-oss-overlay
```

Output must be fresh and separate from the input tree. The tool checks exact
platform.ts/process.ts/license hashes before writing, then produces changed files
under their original paths, the license and ELYTRA-ADAPTATIONS.json with revision,
input/output hashes and the stated adaptation. It accepts upstream's exact CRLF
license checkout as well as the pinned LF fixture; preserve LF TypeScript source.
It validates its changed inputs, not every file in a checkout.

This output is a partial source overlay to merge into a separate application
build tree before compiling Code-OSS. It is not an app package or a launch command.
The source tree and Ladybird remain unmodified. Further adapters must be declared,
hash guarded, reviewed and covered by actual upstream behavior tests.

## Verify the implemented slice

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/runtime-js
python3 tooling/tests/test_code_oss_platform.py --mite build/release-mite/bin/mite
```

The test compiles actual pinned platform/process/nls modules with the existing
build-only esbuild 0.25.12, then compares unmodified Node, adapted Node and adapted
Mite. Native OS/arch, environment/cwd, configured locales and timer scheduling
pass locally on macOS. An unadapted Mite negative control proves why this change
is needed. Missing/duplicate seams, source/license drift and output overlap are
refused; original sources and license bytes are checked for preservation.

The shared bootstrap-node module and performance module have separate tests.
Compile-cache/V8 assumptions, desktop identity/API detection, main.ts/mainImpl,
public binary/event/port transport and the actual workbench remain unfinished.
