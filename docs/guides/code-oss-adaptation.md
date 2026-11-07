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
pass on macOS and in the native Linux/Windows component gates; the latest full
Windows run still fails on Node HTTP/HTTPS reference crashes. An unadapted Mite
negative control proves why this change
is needed. Missing/duplicate seams, source/license drift and output overlap are
refused; original sources and license bytes are checked for preservation.

## Disable the optional Node/V8 cache for LibJS

```sh
python3 tooling/code_oss/overlay.py --source build/code-oss-source --output build/code-oss-libjs-overlay --libjs-cache
node tooling/code_oss/compile.mjs --source build/code-oss-source --overlay build/code-oss-libjs-overlay --output build/code-oss-diagnostic
build/release-mite/bin/mite tooling/code_oss/probe.mjs build/code-oss-diagnostic
```

Both outputs must be fresh. The extra overlay redirects nodeCompileCache's two
engine-specific imports to the owned application policy and adds three explicit
unsupported-mode guards. Ordinary development/missing/disabled startup paths keep
the upstream logic. Cache generation, V8 measurements and cache readiness waits
fail explicitly; there is no fake bytecode, statistics or .ready marker.

The compiler checks overlay hashes, compiles actual source with pinned esbuild
0.25.12, retains real imports and records ELYTRA-COMPILE.json. It copies source
and Elytra notices. It compiles .ts/.js/.json under src and root product/package
metadata, with legacy decorators/class-field settings and ES2024 output. It does
not type-check, install npm dependencies or build workbench assets; this output is
a diagnostic source tree, not a production app package.

The standalone-Mite probe currently exits **1**, recording the real original-fs
import failure at mainImpl after cache initialization. original-fs is already a
Firefly-only module; standalone Mite deliberately does not provide it. The failure
remains a failure and must not be reported as a desktop launch. Run the next
startup gates in the actual Firefly host with its required exports/options.

```sh
python3 tooling/tests/test_code_oss_cache.py --mite build/release-mite/bin/mite
```

This uses real pinned upstream cache/static dependency modules, checks the actual
status/logging branches, explicit refusals, no fabricated files/statistics and
unchanged originals. [ADR 0106](../architecture/0106-code-oss-libjs-cache-policy.md)
records the policy. Mite's public node:v8/Node cache APIs remain unavailable.
The workbench, desktop identity/API, transitive bootstrap and public binary/event/
port transport remain unfinished.
