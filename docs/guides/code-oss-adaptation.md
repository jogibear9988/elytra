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
The workbench, complete desktop identity/API, transitive bootstrap and public
port/stream transport remain unfinished. Later native document-channel gates now
prove bounded public binary/event IPC; [sandbox process](../api/sandbox-process.md)
also has local argv/config IPC/environment acceptance. These do not prove a launch.

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
protocol privilege allowServiceWorkers (exit 1). The original sandbox branch is unchanged: this adaptation
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
reaches unsupported protocol privilege allowServiceWorkers; both preserve exit 1 after registerHooks/promisify. This is not a desktop launch. See the [CLI contract](../api/firefly-cli.md).


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

## Experimental local desktop

This source adaptation is **not a working editor**. It is an explicit experiment
for reaching the first local workbench while keeping webviews, remote resources
and engine code caches unavailable. It does not implement service workers or
change Ladybird. Normal Node/Electron execution retains the original branches.
See [ADR 0149](../architecture/0149-code-oss-local-desktop.md).

Prepare the pinned source and root production dependencies as above. Use fresh
output paths (substitute the path of your completed dependency installation):

```sh
python3 tooling/code_oss/overlay.py --source build/code-oss-source --output build/code-oss-local-overlay --libjs-cache --libjs-engine --local-desktop
node tooling/code_oss/compile.mjs --source build/code-oss-source --overlay build/code-oss-local-overlay --dependencies build/code-oss-dependencies --output build/code-oss-local
mkdir -p build/code-oss-local-user-data
ELYTRA_CODE_OSS_LOCAL_DESKTOP=1 build/renderer/bin/firefly --electron-compat tooling/code_oss/probe.mjs build/code-oss-local --firefly-ready --user-data-dir=build/code-oss-local-user-data
python3 tooling/tests/test_code_oss_local_desktop.py --mite build/release-mite/bin/mite
```

The build option requires both earlier cache/engine options. The runtime flag is
captured once from the truthful LibJS main/preload host. Missing/0 preserves the
original path; invalid values reject on LibJS. Node/Electron ignores the flag.
The enabled main host prints a notice and exposes a frozen diagnostic status
process._vscodeLibJSLocalDesktopStatus. No Node/V8/Electron identity is invented.

Only the original vscode-file registration remains, with the implemented standard,
secure, fetch and CORS privileges. The source scheme declarations are checked
against the audited contract before selecting it. Webview creation and main webview
operations explicitly reject. Remote CLI, resolver and resource-provider entry points
reject; managed remote protocol/channel initialization is omitted. Actual engine
cache-path creation is disabled. This is a limitation of this application mode,
not broader Electron API support. Full webviews/remote remain required later.

Local macOS evidence: the actual compiled 11,536-source tree with prepared root
production packages completes the entry import, and the real app ready callback
runs. The following asynchronous initialization reports missing generated
src/nls.messages.json and unsupported node:zlib. No workbench is opened. The
--firefly-ready probe allows three seconds for that callback and always exits 1
with workbenchAccepted:false; neither readiness nor an import pass proves desktop
acceptance. The normal --firefly probe still exits immediately after the import.
Without the runtime opt-in, the same tree still rejects allowServiceWorkers.

The new native CTest is mite-code-oss-local-desktop. It verifies pinned fingerprints,
original byte preservation outside declared changes, drift/duplicate/seam refusal,
fresh output validation and real Node/Mite enabled/disabled/config/feature/CLI/cache
policy behavior. These are component tests, not service/workbench or platform GUI
acceptance. Fresh Linux/Windows runs remain required.

## Generate the indexed English NLS catalog

The compiler now accepts --nls to run the pinned Code-OSS binding analyzer before
esbuild, replace localize/localize2 key arguments with matching global indices and
generate the actual English messages/keys/metadata/JavaScript catalogs. This is not
an empty fallback. Original English arguments and translator comments are retained.
The build fails on unsupported dynamic keys/messages or generated-path collisions
before creating output. Without --nls the earlier diagnostic build remains unchanged.

Install the updated locked build tools and use another fresh output directory:

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/code_oss
node tooling/code_oss/compile.mjs --source build/code-oss-source --overlay build/code-oss-local-overlay --dependencies build/code-oss-dependencies --nls --output build/code-oss-local-nls
ELYTRA_CODE_OSS_LOCAL_DESKTOP=1 build/renderer/bin/firefly --electron-compat tooling/code_oss/probe.mjs build/code-oss-local-nls --firefly-ready --user-data-dir=build/code-oss-local-user-data
python3 tooling/tests/test_code_oss_nls.py --mite build/release-mite/bin/mite
```

Local evidence: the full tree generates 34,273 messages in 2,501 modules. The actual
ready/bootstrap branch loads them without the prior missing nls.messages.json
error, then stops at unsupported node:zlib. The bounded probe still exits 1;
there is no working workbench. ELYTRA-COMPILE.json records counts, tool provenance
and generated artifact hashes. The standalone non-GUI CTest is mite-code-oss-nls.
The pinned TypeScript package is a build dependency only; Mite still embeds LibJS.
See [ADR 0150](../architecture/0150-code-oss-indexed-nls.md) for exact scope and limits.

## Verify the actual IPC compression wrappers

The native node:zlib slice now implements the required stateful raw compression
streams and flush. Against a real compiled source tree with the engine/platform
adaptations, execute the unchanged pinned Code-OSS compression classes using its
real compiled base services:

```sh
node tooling/code_oss/probe-zlib.mjs build/code-oss-local-nls build/release-mite/bin/mite
python3 tooling/tests/test_zlib.py --mite build/release-mite/bin/mite --host build/release-mite/bin/mite-embedded-host
```

This local Node/Mite component probe passes repeated messages with context takeover.
The actual ready/main graph now passes the zlib import and stops at unsupported
node:assert. The bounded ready probe still exits 1; no workbench is accepted. See
[node:zlib scope and bounds](../api/zlib.md). The previous NLS-only observation above
remains the historical result for a runtime without this newer native slice.

## Verify actual argument assertions

The new node:assert slice provides real callable assertions, error metadata,
equality and exception checks. Verify Code-OSS's unchanged main/CLI argument parser
against the real compiled source tree:

```sh
node tooling/code_oss/probe-argv.mjs build/code-oss-local-nls build/release-mite/bin/mite
python3 tooling/tests/test_assert.py --mite build/release-mite/bin/mite --host build/release-mite/bin/mite-embedded-host
```

Valid --goto values pass and malformed line/column input throws AssertionError under
both Node and Mite. The rebuilt actual Firefly ready/main graph passes assert and
now stops at unsupported node:readline. The bounded diagnostic still exits 1, with
no accepted workbench. See the [assert contract](../api/assert.md) for partial API
scope and explicit unsupported comparisons. Earlier blocker observations above
remain historical results for older runtime builds.

## Read real file lines and inspect the next port blocker

The native FileHandle stream and nonterminal readline slice now runs a real bounded
file-to-lines workflow. Run the non-GUI acceptance and actual pinned OS-release module:

```sh
python3 tooling/tests/test_readline.py --mite build/release-mite/bin/mite --host build/release-mite/bin/mite-embedded-host
python3 tooling/tests/test_code_oss_os_release.py --mite build/release-mite/bin/mite
```

The second gate uses the actual compiled module, truthful platform identity and real
native filesystem. On macOS it accepts the early platform return; only a native
Linux execution can accept the Linux identifier-file branch. No injected fake
platform or replacement filesystem/parser is used. The separate real-file fixture
reads 30,000 UTF-8 lines with pressure and descriptor cleanup under Node/Mite/native GC.

The rebuilt actual ready/main graph passes readline. With the main-realm port
slice below, it also passes MessageChannelMain/utilityProcess/safeStorage/WebContentsView/webContents and next fails linking
child_process.fork. The [real LibJS utility payload slice](../api/utility-process.md) has six
passing native boundary tests, with local public Firefly/Electron lifecycle and binary payload acceptance passing. Actual Code-OSS
utility execution, authentication forwarding and native port handoff remain required;
main-only channels cannot supply those connections. The bounded probe still exits 1,
with no accepted workbench. See [readline](../api/readline.md) and
[file streams](../api/file-read-stream.md) for explicit scope and limits.

## Verify the actual MessagePort protocol

The main-realm MessageChannelMain ownership core now supports real copied binary
frames, start/FIFO, endpoint moves and disconnect. Run the actual pinned Code-OSS
Protocol/once helper using its real compiled base services and public desktop ports:

```sh
node tooling/code_oss/probe-message-ports.mjs build/code-oss-local-nls build/renderer/bin/firefly
python3 tooling/tests/test_message_ports.py --firefly build/renderer/bin/firefly
cmake --build build/renderer --target check-firefly-message-port-lifetime --parallel 3
```

An optional final argument to probe-message-ports.mjs selects a real Electron binary
for the same component comparison. The public runner also accepts --electron. The
lifetime target uses actual Mite/LibJS GC and native BridgeData, without QApplication
or windows. Both new gates are dependencies of check-firefly and hosted Linux native
acceptance; fresh hosted execution is not inferred from local success.

Local actual Protocol binary-frame/FIFO/snapshot/echo/disconnect and moved once-helper
acceptance passes on Electron/Firefly. The real ready/main graph now passes the
utilityProcess/safeStorage imports and fails at missing child_process.fork. The real LibJS utility
fork/parentPort payload implementation has six passing native boundary tests; local
Firefly/Electron public lifecycle, payload and parent-reaping acceptance also passes. This does not run the actual Code-OSS utility services. They
require authentication forwarding and utilityProcess.postMessage with transferred
ports, then webContents/ipcRenderer transfers with event.ports; these native endpoint
handoffs remain unsupported. No workbench is accepted. See the precise
[port scope and bounds](../api/message-channel-main.md).


## Verify real utility children

```sh
python3 tooling/tests/test_utility_process.py --firefly build/renderer/bin/firefly
python3 firefly/tests/utility-process/frame-boundaries.py --firefly build/renderer/bin/firefly
cmake --build build/renderer --target check-firefly-utility-process --parallel 3
```

The public runner optionally accepts --electron with an actual Electron binary.
It opens no windows and proves native child identity, args/env/cwd, binary snapshot
messaging, real stdout/stderr, final data/exit, fast failures and parent teardown.
The POSIX headless fixture rejects malformed/truncated frames and an absent inherited
channel before entry. These tests do not establish cross-process MessagePort transfer.

## Verify the actual encryption service

The unchanged compiled Code-OSS EncryptionMainService now passes real native
safeStorage write/read across two OS processes. The runner verifies its original
source pins and 39 compiled dependencies, including the actual platform/provider
classification and Buffer JSON serialization:

```sh
node tooling/code_oss/probe-safe-storage.mjs build/code-oss-nls-final build/renderer/bin/firefly-safe-storage-probe --test-keychain '{directory}/test.keychain' --test-profile '{directory}/profile'
```

This host uses the production backend with an isolated temporary OS keychain and a
minimal ready/command-line app fixture. It does not modify the user's Login
keychain or accept production post-ready app lifecycle. Real Firefly/Electron
comparison separately accepts pre-ready guards only; post-ready Electron crypto
is unverified. The current actual ready graph passes safeStorage and next fails
linking child_process.fork. Native utility/renderer endpoint transfer, demanded auth
forwarding and actual Code-OSS shared-service execution remain required; no
workbench is accepted. See [storage scope](../api/safe-storage.md), including the
Firefly-specific ciphertext and unavailable migration/async/platform providers.

## Native child views and the next observed import

The shared public WebContentsView fixture passes in Firefly's test-only offscreen
frontend and Electron 45, and the same complete fixture passes in a hidden batch
using the production Cocoa frontend. Real pages/PIDs, viewport changes, native
ordering/ownership/reparenting, detached navigation, registry and binary IPC/close
are accepted. No on-screen GPU/compositor, accessibility or performance claim is
made. The static offscreen plugin is not shipped and supplies no production desktop
fallback; native Linux/XCB and Windows execution remain unverified.

The actual prepared Code-OSS local desktop/NLS entry now reaches app ready and
passes WebContentsView/webContents imports on the production objects. Its next
missing export is child_process.fork. The metadata-only desktop source and empty-image
APIs are implemented with separate policy/controlled-host/preflight acceptance;
valid OS enumeration and media binding remain unaccepted. Notification and genuine read-only macOS powerMonitor component gates now pass;
physical power transitions and actual OS notification delivery remain unaccepted.
Owned macOS powerSaveBlocker assertions now pass native lifecycle/readback and
controlled failure gates; native shell dispatch and genuine owned-file Trash/restore now pass; continue with
native scalar preferences and media-status gates now pass. Continue with genuine Mite fork/JSON IPC and rerun the graph. Integrated BrowserView still
requires unsupported session/subframe/CDP services; native ports/authentication and
a working VS Code window remain missing. See [view scope](../api/web-contents-view.md).
