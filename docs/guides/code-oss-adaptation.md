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
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/code_oss
python3 tooling/tests/test_code_oss_platform.py --mite build/release-mite/bin/mite
```

The test compiles actual pinned platform/process/nls modules with the existing
build-only esbuild 0.25.12, then compares unmodified Node, adapted Node and adapted
Mite. Native OS/arch, environment/cwd, configured locales and timer scheduling
pass on macOS and in the native Linux/Windows component gates; the native 5735b91 Linux/Windows full runs pass. Earlier Windows Node HTTP/HTTPS
reference crashes remain undiagnosed; the later success does not establish a fix. An unadapted Mite
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

## Separate implicit Chromium defaults explicitly

The normal overlay without this extra option still stops at the incompatible
Chromium enable-features append. Opt in to the audited application adaptation:

```sh
python3 tooling/code_oss/overlay.py --source build/code-oss-source --output build/code-oss-libjs-engine-overlay --libjs-cache --libjs-engine
node tooling/code_oss/compile.mjs --source build/code-oss-source --overlay build/code-oss-libjs-engine-overlay --dependencies build/code-oss-dependencies --output build/code-oss-engine-diagnostic
build/renderer/bin/firefly --electron-compat tooling/code_oss/probe.mjs build/code-oss-engine-diagnostic --firefly
```

Prepare the frozen root dependencies as described below first; all output paths
must be fresh. This only changes declared insertion seams in pinned mainImpl.ts.
Its default Chromium/Blink/Electron tuning block remains intact in the ordinary
Node/Electron branch. Actual Mite records omitted implicit tuning names/reasons
in a deeply frozen process._vscodeLibJSEngineStatus; the probe emits that status.
Its scope is engine defaults only, and renderer compatibility remains unestablished.
See the [per-feature audit](../architecture/0131-code-oss-engine-defaults.md).

The audited user-supplied feature/V8/proxy/rendering/debug/portal options and attempts to disable
the sandbox fail with ERR_CODE_OSS_LIBJS_ENGINE_OPTION before upstream config/argv
mutations. This includes native switch presence even when a value is empty/false.
The minimist default sandbox alias is handled separately from actual raw presence.
Application logging, proposed API and in-memory secret-storage argv behavior is
preserved. No engine flag is advertised as supported by Firefly through this overlay.

```sh
python3 tooling/tests/test_code_oss_engine.py --mite build/release-mite/bin/mite
```

The native CTest entry is mite-code-oss-engine. Local macOS Node/Mite exact-source,
original/adapted Node behavior and eight negative policy cases pass. With the real
prepared dependencies, the actual Firefly-host entry now reaches missing
protocol privilege corsEnabled (exit 1). The original sandbox branch is unchanged: this adaptation
does not enable/disable sandboxing or claim a workbench launch. Fresh native
Linux/Windows engine-overlay acceptance remains required.

## Probe the actual Firefly host

```sh
build/renderer/bin/firefly --electron-compat tooling/code_oss/probe.mjs build/code-oss-diagnostic --firefly
```

The final --firefly argument requests explicit diagnostic shutdown, preserving a
nonzero result on failure. This is a real host run with original-fs and the
implemented Electron alias. Without prepared dependencies, the trace stops at missing minimist. With the
root production installation below, the actual trace without --libjs-engine reaches
unsupported Chromium enable-features. With the explicit engine overlay above, it
reaches unsupported protocol privilege corsEnabled; both preserve exit 1 after registerHooks/promisify. This is not a desktop launch. See the [CLI contract](../api/firefly-cli.md).


## Prepare actual root production dependencies

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/code_oss
node tooling/code_oss/dependencies.mjs --output build/code-oss-dependencies
node tooling/code_oss/compile.mjs --source build/code-oss-source --overlay build/code-oss-libjs-overlay --dependencies build/code-oss-dependencies --output build/code-oss-with-dependencies
build/renderer/bin/firefly --electron-compat tooling/code_oss/probe.mjs build/code-oss-with-dependencies --firefly
```

Create build first if absent. Each output must be fresh. Default preparation uses
vendored exact upstream inputs; --source may select the full pinned checkout.
The tool verifies package/lock/workspace/hook/native-build/patch/license inputs,
then runs the application's pnpm 12.16.0 with --prod --ignore-scripts
--frozen-lockfile --filter . in the separate output tree. Dependency lifecycle
scripts are disabled; the pinned upstream configuration hook executes. Node/pnpm
are build tools, not application runtime dependencies.

ELYTRA-DEPENDENCIES.json distinguishes inventory-only (--inventory-only), failed
and completed installs. The compiler accepts only a completed same-source/native
platform tree, preserves contained relative links, copies modules and records
provenance. It writes ELYTRA-COMPILE.json only after successful copying. Owned
reports are not attestations against later manual module-tree changes.

Scope is **root production dependencies**, excluding root devDependencies, other
workspace importers and extension/separate install roots. The macOS ARM64 scan
finds 71 direct dependencies, 258 frozen snapshots, 231 installed package directories
and 27 candidates with native/WASM evidence. It inspects data without loading
packages; absence of markers does not certify compatibility. PTY, watcher,
SQLite, keymap and other Node bindings still need real engine-independent adapters;
Mite has no native Node addon/N-API loader. Full published packages are retained
for diagnosis; this tree's size is not a release/framework size measurement.

The pinned Code-OSS root dependency workflow runs this frozen install on native
macOS/Linux/Windows and retains its inventory. The native frozen installs pass on macOS/Linux/Windows at adc00d0 in
[run 42389926763](https://github.com/jogibear9988/elytra/actions/runs/42389926763).
Installation success is separate from actual Mite package and desktop acceptance.

```sh
python3 tooling/tests/test_code_oss_dependencies.py --mite build/release-mite/bin/mite
```

This non-GUI gate verifies both input layouts, drift/incomplete-install refusal and
minimist integrity. It extracts the actual compiled parseCLIArgs function from
pinned mainImpl.ts, preserving its algorithm, and compares 14 argument cases under
Node and Mite using minimist 1.2.8. It does not simulate an application launch.
See [ADR 0108](../architecture/0108-code-oss-root-dependencies.md).
