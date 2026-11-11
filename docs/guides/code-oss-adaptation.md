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
for reaching the first local workbench while keeping webviews, remote resources,
display capture and engine code caches unavailable. It does not implement service workers or
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
not broader Electron API support. Full webviews/remote/capture remain required later.

Local macOS evidence: the actual compiled 11,536-source tree with prepared root
production packages completes the entry import, and the real app ready callback
runs. Those first diagnostic failures were the generated NLS catalog and node:zlib;
later slices resolved both. With NLS and the explicit policy package adaptation,
the current fresh tree reaches session.defaultSession.webRequest.onBeforeRequest
after the optional capture deferral below. No workbench is opened. The
--firefly-ready probe allows three seconds for that callback and always exits 1
with workbenchAccepted:false; neither readiness nor an import pass proves desktop
acceptance. The normal --firefly probe still exits immediately after the import.
Without the runtime opt-in, the same tree still rejects allowServiceWorkers.

The new native CTest is mite-code-oss-local-desktop. It verifies pinned fingerprints,
original byte preservation outside declared changes, drift/duplicate/seam refusal,
fresh output validation and real Node/Mite enabled/disabled/config/feature/CLI/cache
policy behavior. These are component tests, not service/workbench or platform GUI
acceptance. Fresh Linux/Windows runs remain required.

## Optional display capture deferral

The pinned engine does not implement getDisplayMedia. The explicit local desktop
mode therefore skips only CodeApplication.configureSession's original
setDisplayMediaRequestHandler registration. It does not add a public Session
setter. Both permission handlers and all security-relevant webRequest filters
remain unchanged; reaching the latter now exposes the next genuine missing API.

The pinned NativeRecordingService reports unsupported in this mode. Recording,
OS capture authorization and capture-settings entry points fail explicitly with
ERR_CODE_OSS_LOCAL_DESKTOP_UNSUPPORTED before capture or OS collaborators run.
The read-only permission-status query remains intact. Ordinary Node/Electron and
disabled local-mode paths retain their original method bodies and behavior.
The frozen process diagnostic includes display-capture in its disabled list.

The source-policy test above verifies pinned hashes, exact reversal outside owned
seams and missing/duplicate-seam rejection. It executes actual registration and
recording method bodies on Node and Mite with controlled collaborators, including
default/control branches and refusal before capture, prompts or settings launches.
These are policy tests, not display/device acceptance. Ladybird remains unmodified.

The fresh build/code-oss-capture-final contains 11,536 compiled sources. The actual
entry completes and app ready is observed with display capture disclosed as
disabled. Its first failure is session.defaultSession.webRequest.onBeforeRequest
(undefined webRequest); Error.captureStackTrace is a secondary reporting gap.
Native frame metadata and authenticated filtering still need acceptance; no
workbench, workspace or native display capture is claimed. See
[ADR 0179](../architecture/0179-optional-display-capture.md).

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
slice below, it also passes MessageChannelMain/utilityProcess/safeStorage/WebContentsView/webContents,
native filesystem watchers and the protocol service. The explicit native policy
package adaptation below advances startup through local IPC to display-media configuration after document-bound permission registration. The [real LibJS utility payload slice](../api/utility-process.md) has six
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
utilityProcess/safeStorage imports and fails at missing fs.watchFile. The real LibJS utility
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
is unverified. The current actual ready graph passes safeStorage, native filesystem
watchers and the protocol service. The native policy adaptation below advances
startup further; current blockers are tracked in the compatibility matrix. Native utility/renderer endpoint transfer, demanded auth
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
passes WebContentsView/webContents imports and the protocol service on the production
objects. Native fs.watchFile/fs.watch have separate local acceptance. The explicit
policy package adaptation below uses a genuine native provider instead of loading
the package's V8 helper/Node addon; startup now passes local IPC and reaches display-media configuration after document-bound permission registration. The metadata-only desktop source and empty-image
APIs are implemented with separate policy/controlled-host/preflight acceptance;
valid OS enumeration and media binding remain unaccepted. Notification and genuine read-only macOS powerMonitor component gates now pass;
physical power transitions and actual OS notification delivery remain unaccepted.
Owned macOS powerSaveBlocker assertions now pass native lifecycle/readback and
controlled failure gates; native shell dispatch and genuine owned-file Trash/restore now pass; continue with
native scalar preferences and media-status gates now pass. Genuine fork/JSON IPC and the unchanged Code-OSS IPC component now pass. Continue with native file polling and rerun the graph. Integrated BrowserView still
requires unsupported session/subframe/CDP services; native ports/authentication and
a working VS Code window remain missing. See [view scope](../api/web-contents-view.md).


## Native policy package adapter

The default @vscode/policy-watcher 1.7.0 entry loads a Node/N-API addon using a V8
stack helper. Mite does not supply either interface. The opt-in build flag below
adapts only that exact package's entry in a fresh compiled copy. Under Mite it uses
[mite:policy-watcher](../api/policy-watcher.md); ordinary Node/Electron keep their
original native-addon branch. Original prepared dependencies and application source
are unchanged; package/source/adapter/output hashes are recorded in ELYTRA-COMPILE.json.
A different package version or unexpected original entry fails the build.

After preparing the pinned dependencies as described above:

```sh
python3 tooling/code_oss/overlay.py --source build/code-oss-source --output build/code-oss-policy-overlay --libjs-cache --libjs-engine --local-desktop
node tooling/code_oss/compile.mjs --source build/code-oss-source --overlay build/code-oss-policy-overlay --dependencies build/code-oss-dependencies --nls --mite-policy-watcher --output build/code-oss-policy
node tooling/code_oss/probe-policy.mjs build/code-oss-policy build/release-mite/bin/mite
```

Use fresh output paths. The consumer probe verifies the unchanged NativePolicyService
and NativeManagedSettingsService and 46 compiled dependencies, then initializes and
disposes genuine native watchers against a unique unused preferences domain. Local
macOS passes without preference writes, injected empty snapshots or GUI tests.
Windows/Linux providers are unavailable; actual MDM change delivery is unaccepted.
General .node/N-API and Error.captureStackTrace remain unsupported.

With this explicit package adaptation and the local-desktop/NLS mode, the actual
main-ready diagnostic passes ipc.net.serve/main.claimInstance and reaches
CodeApplication.configureSession. Both permission setters now pass genuine
document-bound native/renderer acceptance; startup next fails at the missing
session.defaultSession.setDisplayMediaRequestHandler. An accepted workbench and
open/edit/save remain outstanding; see the
[current Code-OSS requirements](../compatibility/vscode.md).

## Local IPC channel acceptance

```sh
node tooling/code_oss/probe-local-ipc.mjs build/code-oss-policy build/release-mite/bin/mite
```

The probe verifies the unchanged pinned ipc.net.ts input and 41 compiled dependency
outputs. It runs genuine channel framing, client context and VSBuffer request/reply
in separate OS processes for Mite/Mite, Mite/Node and Node/Mite. The binary payload
exceeds 256 KiB; both channel and listener dispose naturally. Local macOS passes.
This is distinct from Firefly utility/renderer endpoint transfer and does not
establish native Windows/Linux pipe execution or a workbench launch. See
[the local stream contract](../api/net.md#local-streams).


## Longer startup diagnostics

The ready probe defaults to a three-second observation and exits nonzero even
when it reaches readiness. To look for later main-service failures, set
`ELYTRA_CODE_OSS_OBSERVE_MS=15000` (accepted range 1000–60000). This only changes
the owned diagnostic's deadline; it does not adapt Code-OSS or accept a desktop.
Create the selected user-data directory before launching and retain the explicit
`ELYTRA_CODE_OSS_LOCAL_DESKTOP=1` opt-in. The end record lists actual native window
IDs and URLs if the deadline is reached; workbenchAccepted always remains false.

With native request interception, the longer local trace gets past configureSession
but fails in subsequent services at synchronous ESM require and uncaughtException
registration; missing Error.captureStackTrace also disrupts error reporting. See
the current [VS Code audit](../compatibility/vscode.md). Utility request forwarding
and a real workbench/open/edit/save run are still required.


## Disabled V8 window cache preference

The experimental local-desktop overlay also adapts the exact pinned
`windowImpl.ts` preference initializer. On ordinary Node/Electron it preserves
`v8CacheOptions` unchanged. On Mite it omits only the explicit `none` value,
after checking the local-desktop opt-in, disabled engine code cache and LibJS
engine policy. An enabled cache request or inconsistent policy rejects with
`ERR_CODE_OSS_LIBJS_ENGINE_OPTION` before publishing the diagnostic record.
This does not implement V8/Blink caches or admit that option in Firefly's general
BrowserWindow API. All other renderer preferences remain intact and validated.

The non-enumerable `process._vscodeLibJSWindowPreferencesStatus` record describes
the omitted preference and why it is unavailable. The probe includes this record
at the end of its observation, including preferences assembled asynchronously
after the main entry completes. It still reports `workbenchAccepted: false`.

The source transform checks the exact pinned SHA-256 and unique seams before
writing any output. The tests reverse the transform to the original bytes and
execute the actual pinned preference initializer under Node and Mite, checking
accessor evaluation order and preservation of the remaining security settings:

```sh
python3 tooling/tests/test_code_oss_window_preferences.py --mite build/release-mite/bin/mite
```

A fresh local macOS diagnostic with this adaptation passes the prior cache-option
rejection and reaches app readiness. With disabled native spelling support in
Firefly now passes `spellcheck:false`, native `zoomFactor`, autoplay and the
already-enabled HighlightAPI request. The latest direct trace creates a native
window but stops during optional TouchBar initialization before loading the
workbench URL. Native spdlog bindings remain unavailable.
This is startup progress, not accepted workbench or file-editing support.

## Deprecated synchronous script paste

The explicit local-desktop overlay also adapts the pinned `windows.ts`
`enableDeprecatedPaste: true` assignment. Ordinary Node/Electron keeps the value.
Verified Mite/LibJS local-desktop mode omits this one application opt-in and
publishes immutable `process._vscodeLibJSLegacyPasteStatus` only when it does so.
Firefly's general `enableDeprecatedPaste: true` preference remains unsupported.

This permits startup without claiming synchronous `document.execCommand('paste')`
clipboard access. Electron's deprecated mode performs an additional synchronous
permission check; LibWeb's real native/user paste action is a separate path.
The desktop Code-OSS clipboard service calls `webContents.paste()`, so genuine
integration of that method is still required. The browser-only text fallback
must not be claimed for this desktop experiment. No clipboard delivery or editor
acceptance follows from omitting the preference.

## Stage the real development renderer without another application copy

The diagnostic compiler can include upstream HTML, CSS, images, fonts and WASM:

```sh
node tooling/code_oss/compile.mjs --source build/code-oss-source --overlay build/code-oss-start-overlay --dependencies build/code-oss-runtime-dependencies-v2 --output build/code-oss-window-cache-final --nls --mite-policy-watcher --mite-native-keymap --mite-sqlite --mite-deviceid --renderer-assets --refresh
```

Use --refresh only for an existing output from this compiler; omit it for a fresh
output. The tool checks every previously recorded output before writing, rejects
edited files and unowned collisions, preserves unchanged files and Finder data,
and reuses installed dependencies. New paths cannot follow unowned parent symlinks.
It publishes the completed manifest last. One previous manifest is retained for
inspection; an interrupted refresh cannot claim the old build is complete.

The renderer-assets mode stages exact production asset bytes and the actual
locked codicon font. A relative internal out-to-src alias preserves the upstream
development bootstrap path without duplicating the module tree. Hosts unable to
create this alias fail explicitly; this is not native Windows acceptance.
Upstream's real CSS development service/import-map loader is retained. CSS is not
replaced with empty modules. Successful staging does not establish a rendered
workbench; runtime prerequisites such as CSS module discovery still need a genuine
application start.

## Optional TouchBar in the local editor experiment

Verified local Mite/LibJS mode skips the pinned Code-OSS TouchBar constructor and
records process._vscodeLibJSTouchBarStatus. General Firefly TouchBar remains
unsupported; unexpected Code-OSS update requests fail explicitly with
ERR_CODE_OSS_LIBJS_TOUCHBAR_UNAVAILABLE. Ordinary Node/Electron behavior is
unchanged. This application opt-out enables the local editor experiment without
claiming native TouchBar hardware support.

## Development CSS discovery in the local LibJS stage

The renderer-assets build records the exact sorted paths of its 508 production
stylesheets in src/elytra-css-modules.json, with its hash in the compile manifest.
The pinned local CSS development service reads this inventory before requesting
the absent native ripgrep executable. Missing/malformed data fails explicitly.
Ordinary Node/Electron retain their original discovery path. The real workbench
import-map loader still appends actual stylesheet links; no CSS module is erased.
This does not provide ripgrep search for user files or claim editor acceptance.

## Select the prepared development renderer

The source stage is a development renderer. Set VSCODE_DEV=1 together with
ELYTRA_CODE_OSS_LOCAL_DESKTOP=1, and leave VSCODE_DEV_SERVER_URL unset. Otherwise
Code-OSS selects production workbench.html, expects a bundled stylesheet that is
not produced here, and skips development CSS discovery. No bundled production
renderer is implied by this source compiler.

Use --native-title-bar with the diagnostic probe to create an isolated profile.
In explicit local Mite mode that owned profile also disables keyboard.touchbar.enabled
so the renderer does not request the intentionally omitted optional TouchBar.
Existing user profiles are never overwritten. A reused owned profile needs both
window.titleBarStyle:native and keyboard.touchbar.enabled:false in User/settings.json.

The local remote opt-out covers both channel setup and the afterWindowOpen protocol
handler registration. Actual remote requests remain rejected. The local Darwin
updater skips native subscriptions only after the genuine base service reaches
Disabled; an unexpected enabled state fails explicitly. Neither change implements
remote resources or native application updating.

## Native package adapters for the local editor

Use --mite-native-keymap and --mite-sqlite with the compiler for the exact audited
native-keymap 3.3.9 and @vscode/sqlite3 5.1.12-vscode packages. Mite selects real OS
keyboard-layout queries and real worker-thread SQLite storage; Node/Electron keeps
its original addon path. No Node, V8 or N-API runtime is installed into Mite.
The keyboard provider currently supports macOS only. SQLite has focused macOS
on-disk/transaction/reopen acceptance; other platform execution remains unverified.

When refreshing a stage, retain all its previously selected adapter flags. The
compiler validates old adapters and original newly selected loaders/package.json
before writing and does not copy node_modules again. See
[the native package decision](../architecture/0192-native-code-oss-packages.md).

## Optional utility authentication forwarding

The pinned local desktop adapter disables the optional
respondToAuthRequestsFromMainProcess opt-in only after verifying the Mite/LibJS,
local-desktop and engine-policy identities. Ordinary Node/Electron retains the
original configuration; malformed or unverified local options fail explicitly.
When a true request is omitted, process._vscodeLibJSUtilityAuthStatus records an
immutable unavailable status. Undefined/false does not publish an omission. This
permits local utility startup without claiming utility HTTP authentication
forwarding, authenticated remote/proxy workflows or full editor acceptance. The
general Firefly option remains unsupported. Upstream source bytes outside the
exact import/configuration seam are preserved and reversible.

## Test-only offscreen renderer diagnostics on macOS

To diagnose the real renderer without repeatedly focusing a desktop window, build
`firefly-view-offscreen`. It links the same application objects with Qt's existing
static offscreen platform plugin. In this explicitly selected experiment, the
source adapter omits only the optional first-mouse preference. Native production
input guards remain unchanged; this mode cannot certify a usable desktop editor.

```sh
cmake --build build/renderer --target firefly-view-offscreen
ELYTRA_CODE_OSS_LOCAL_DESKTOP=1 ELYTRA_CODE_OSS_OFFSCREEN_DIAGNOSTIC=1 \
QT_QPA_PLATFORM=offscreen VSCODE_DEV=1 \
build/renderer/bin/firefly-view-offscreen --electron-compat \
  build/code-oss-window-cache-final/src/main.js \
  --user-data-dir /tmp/your-owned-code-profile --disable-crash-reporter
```

Use a private prepared profile with native title-bar and disabled TouchBar settings.
The helper requires the exact diagnostic executable basename, explicit flag and
platform, and verified local engine/cache policy. Invalid combinations fail. The
immutable `_vscodeLibJSOffscreenDiagnosticStatus` says `desktopAccepted:false` and
names the omitted input policy. Normal Node/Electron and production Mite preserve
the original preference. Native handles, first-click behavior and actual desktop
presentation still require a later Cocoa acceptance run. Linux production keeps
its existing Xvfb/X11 requirement.

## Device identity with genuine asynchronous UUID and native storage

The actual desktop entry imports @vscode/deviceid0.1.5, whose CommonJS loader
requires the ESM-only uuid14 package. Use `--mite-deviceid` alongside all existing
package flags when creating or refreshing the stage. This exact pinned package
adaptation defers genuine uuid import into its already asynchronous missing-ID
branch only under Mite. Its storage module uses actual node:fs/promises operations
and recursive mkdir in that same runtime, avoiding the fs-extra/graceful-fs legacy
constants dependency. Existing file paths, persisted IDs and error handling remain.
Node/Electron retain the original synchronous loader and fs-extra branch.

The package version, both changed source files, package manifest and license are
pinned under vendor/deviceid. Compilation records both input/output hashes; refresh
checks both staged files and refuses edited sidecars. A focused actual Node/Mite
fixture generates a real UUID and reads it back from a file in a fresh temporary
HOME. No replacement ID, entropy source or private user storage is used by that
fixture. This adaptation does not add general synchronous require(ESM), legacy
constants or full fs-extra support to Mite.

## Agent-host development inspector

The pinned local LibJS overlay also adapts the AgentHost starter's implicit
5878 inspector default. It keeps the agent process enabled, omitting only the
synthesized `--nolazy` / `--inspect=5878` pair in the verified local/no-cache mode.
The immutable `process._vscodeLibJSAgentHostDebugStatus` discloses inspector
unavailability. Explicit inspect-agenthost / inspect-brk-agenthost requests,
including empty values, fail early. Ordinary Node/Electron and built no-debug
parser results remain unchanged. The exact source and reversible seams are pinned;
this does not make the inspector or complete agent services supported.

## Bounded native startup phases

For an owned offscreen diagnostic, `MITE_MODULE_DIAGNOSTIC=1` emits fixed ESM
entry begin/submitted and pending/fulfilled/rejected Promise-state transitions.
It works in standalone Mite without a desktop dependency. Combine it with
`FIREFLY_IPC_BYTES_DIAGNOSTIC=1` for native utility initialization, parent-handler
installation and at most 32 receive/send-admitted frame counters per endpoint.
These records include only native process/endpoint identifiers, byte counts and
fixed phase labels. They do not print module paths, source, environment contents
or message payloads. A fulfilled entry or admitted frame does not establish service
readiness; use the actual application's handshake as the acceptance criterion.

## Implicit extension-host engine defaults

The pinned localProcessExtensionHost adapter validates the explicit local LibJS
profile before creating a host or probing a debugger port. Extension development,
explicit inspect/break settings, extension tests and V8 profiling remain errors.
Ordinary Node/Electron retains the original flag construction and does not read
the adapter's policy inputs. Local mode omits only the exact generated default
array: IPv4-first DNS ordering, experimental network inspection, and either the
implicit localhost inspector/nolazy pair or inspect-port=0 fallback. It does not
claim those DNS or inspector settings were applied. Unexpected flags fail.

The sandbox uses its genuine firefly/libjs process identity and the owned local
profile record, without inventing a Mite version or main-only engine status.
An immutable renderer-global _vscodeLibJSExtensionHostFlagsStatus reports
inspectorAvailable:false, networkInspectionAvailable:false and
dnsOrderingApplied:false. Separately, the main utility adapter replaces the
pinned allowLoadingUnsignedLibraries:true opt-in with false after its verified
local check; process._vscodeLibJSUtilityUnsignedLibrariesStatus discloses that
unsigned-library relaxation is unavailable. This neither loads NAPI addons nor
changes macOS entitlements. General utility flags remain unsupported; successful
extension-host startup and actual extension activation still require acceptance.
