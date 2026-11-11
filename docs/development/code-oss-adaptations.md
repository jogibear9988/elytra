# Code-OSS adaptation and removal tracker

This is the inventory of the current pinned Code-OSS development overlay and its
six optional package adapters. Temporary application changes are permitted while
the genuine Mite/Firefly capability is developed, provided the reason, replacement
and removal check remain recorded here. An adapted application reaching a later
startup phase does not establish compatibility with unchanged Code-OSS.

The source pin is recorded in `vendor/code-oss-tests/manifest.json`. The overlay
entry point is `tooling/code_oss/overlay.py`; package selection is in
`tooling/code_oss/compile.mjs`. Owned TypeScript helpers live in
`firefly/compat/code-oss/`. This tracker supplements the
[adaptation guide](../guides/code-oss-adaptation.md),
[current compatibility blockers](../compatibility/vscode.md) and
[validation history](../PROGRESS.md). Those records retain the evidence for each
implemented slice; this document does not promote source-policy checks to desktop
acceptance.

## Rules and status

Each active `CA-*` adaptation below has an **open removal TODO**. CA-18 records
the first source-workaround removal; its original-consumer acceptance is still open. “Port” means a genuine alternate
implementation is selected, but the application/package source still differs.
“Deferral” means functionality is unavailable or behavior is intentionally reduced.
“Build” means source compilation, resources or diagnostics, rather than an API
implementation. None is permission to accept unsupported public options silently.

The overlay preserves the original Node/Electron branches. Local deferrals require
the explicit verified Mite/LibJS local-desktop policy and publish unavailable-feature
status where described. Mite must not acquire invented Node/V8 versions to remove
these branches. Permission handlers, webRequest security filters, sandbox requests
and origin/document checks are not optional startup obstacles to strip away.

Removal requires restoring the pinned upstream bytes, removing the corresponding
helper/import and manifest entry, and running the original consumer against the
real replacement. Keep drift/refusal and ordinary Electron controls until that
removal is accepted. A provenance hash proves which bytes ran, not that their
semantics are correct. Do not close these TODOs merely because startup advances.

## Application behavior changes

Source names below are the pinned input aliases used by the overlay; their exact
upstream paths and hashes are in the vendor manifest and transformer constants.

| TODO / status | Current change and why | Genuine replacement and removal validation |
| --- | --- | --- |
| CA-01 — Port | `overlay.py`: `platform.ts` and `process.ts` recognize truthful `versions.mite` alongside `versions.node`. Upstream otherwise misclassifies native Mite execution. | Prefer upstream runtime-neutral native-process detection. Remove the overlay when unmodified platform/process consumers select the correct native OS, environment, locale and scheduling behavior without a fake Node identity. |
| CA-02 — Deferral | `nodeCompileCache.ts`, `compile-cache.ts`: replace Node/V8 compile-cache imports with explicitly disabled LibJS policy; generation, V8 statistics and unsupported readiness operations reject. `local-desktop.ts` omits the cache path. | Provide an actual compatible cache lifecycle backed by LibJS, or upstream an explicit engine-independent disabled-cache mode. Verify original enable/generate/readiness branches and real restart behavior; no fabricated cache files or statistics. |
| CA-03 — Deferral | `engine.py`, `mainImpl.ts`, `engine-policy.ts`: omit audited implicit Chromium/Blink engine-tuning switches on LibJS. Explicit unsupported user engine/debug/security requests still reject. | Implement only meaningful native equivalents, or upstream engine-specific configuration selection. Verify every original switch's intended effect or an explicit upstream alternate-engine branch before removing its omission. Chromium-only knobs cannot be “implemented” as inert Firefly options. |
| CA-04 — Deferral | `window_preferences.py`, `window-impl-main.ts`: omit only `v8CacheOptions: 'none'` under verified disabled-cache local mode. Cache-enabled or other requests reject. | Same real cache or upstream engine-selection work as CA-02. Restore the exact initializer; prove both cache branches and unchanged other webPreferences. |
| CA-05 — Deferral | `window_preferences.py`, `window-defaults-main.ts`: omit `enableDeprecatedPaste: true`. Deprecated synchronous script paste is not implemented; this does not establish native clipboard paste support. | Integrate genuine clipboard/paste routing with document permissions and activation. Restore the initializer and test actual editor paste, denied script access and native menu/keyboard paths. |
| CA-06 — Deferral | `window_preferences.py`, `window-preferences.ts`: skip optional TouchBar initialization; later TouchBar updates reject explicitly. | Implement native TouchBar objects, updates and ownership, or use an upstream supported opt-out. Restore construction and update sites; validate native controls and cleanup. |
| CA-07 — Diagnostic | `libJSFirstMousePreferences`: omit `acceptFirstMouse` only for the exact `firefly-view-offscreen` frontend, offscreen platform and explicit diagnostic flag. Production Cocoa still uses the actual per-view native first-mouse adapter. | Keep this separate from desktop acceptance. Remove when diagnostic fixtures no longer require the Cocoa-only option; validate the unmodified constructor on the selected frontend. No claim that offscreen input tests prove Cocoa behavior. |
| CA-08 — Deferral | `local_desktop.py`: browser/electron webview services reject creation; main webview service omits its protocol provider and rejects find/ignore-shortcuts operations. Webviews are disclosed disabled. | Implement real guest/embedded contents, protocol and origin isolation, IPC, preload and lifecycle semantics. Restore all three services together; validate real webview content, isolation, navigation and disposal. |
| CA-09 — Deferral | `local_desktop.py`: remote authority resolution, canonical remote URIs and actual remote-resource operations reject. The remote loader constructor skips channel registration. URI-provider factory construction is allowed; the returned callback rejects before producing an unsupported URI. `app.ts` omits the disabled remote channel/handler. | Implement real remote authority/channel/resource transport and authentication. Restore resolver, loader and main registrations; validate successful remote bytes plus denied/invalid authorities, cancellation and teardown. Do not move the rejection back to harmless factory creation. |
| CA-10 — Deferral | `local-desktop.ts`: reject remote CLI inputs and register only the audited `vscode-file` scheme privileges, without V8 code-cache privileges. Webview and remote schemes are excluded. | Complete CA-02/08/09 and each actual scheme privilege, then restore the entire original declaration. Verify routing, CORS, service-worker/cache behavior and remote CLI paths independently. |
| CA-11 — Deferral | `local_desktop.py`: omit only `setDisplayMediaRequestHandler` registration in `app.ts`; recording support reports false and recording/permission-settings attempts reject in `nativeRecordingService.ts`. Display capture is disclosed disabled. | Implement genuine display-source selection, OS authorization, capture and cancellation. Restore registration and recording methods; prove actual capture and denied/cancelled paths without weakening other permission handlers. |
| CA-12 — Deferral | `disabled_updater.py`: skip Darwin autoUpdater subscriptions only after upstream updater state is genuinely Disabled. Attempts to request updates remain unsupported. | Implement actual signed update lifecycle, or upstream a supported disabled-service branch. Restore subscription code and verify disabled startup plus explicit enabled update behavior. |
| CA-13 — Deferral | `utility_auth.py`, `utility-process-main.ts`: change the local opt-in `respondToAuthRequestsFromMainProcess: true` to false; disclose missing utility authentication forwarding. | Implement genuine parent/system-session authentication routing with bounded challenge/callback ownership. Restore the true option and validate challenge, cancellation, credentials handling and process exit. No fake authentication callback registration. |
| CA-14 — Deferral | Same utility helper opts out of `allowLoadingUnsignedLibraries: true`. It does not change entitlements or pretend NAPI addons can load. | Establish a real platform/native-library policy if needed, or upstream an explicit alternate-runtime choice. Restore the original option and validate actual library admission/refusal; do not relax code-signing security just to accept the option. |
| CA-15 — Deferral | `shared_process_debug.py`: omit only the pinned implicit development inspector default 5879 and its associated implicit flags. Explicit shared-process debug requests, including empty values, reject. Built/no-debug paths retain their original result. | Implement genuine inspector/debug functionality or use an upstream supported no-debug runtime profile. Restore original argv and test attachment/break/wait behavior; a truthful inactive `inspector.url()` is not a debugger backend. |
| CA-16 — Deferral | `agent_host_debug.py`: same restricted omission for implicit development agent-host inspector 5878. The agent process itself remains enabled. | Same debugger replacement as CA-15; restore original startup and verify the real agent still starts and can be debugged. |
| CA-17 — Deferral | `extension_host_flags.py`: omit audited implicit DNS-order/network-inspection and default inspector flags (development 5870 or built `--inspect-port=0`). Disclose unavailable inspector/network inspection and unapplied requested DNS ordering. Explicit debug/development/test/profiler requests reject. | Implement real DNS-order and debug/network-inspection semantics, or upstream capability-specific startup. Restore exact argv and verify default and explicitly requested behavior; do not fake `versions.node` or accept generic engine flags inertly. |
| CA-18 — Removed; original worker operations verified | Removed `diff_worker.py`, `diff-worker.ts` and their overlay registrations. The pinned `diffComputeService.ts` now keeps its original eager `worker_threads.Worker` import and constructor; no feature-disable status or forced diff error is inserted. | Genuine native worker threads now pass focused independent-VM, message, error, exit and termination checks. The original worker entry passes actual Mite count/detail/hunk calculations, forward/reverse edit reconstruction and joined termination (codeoss-original-diff-all-native.log). Actual Workbench service invocation remains unaccepted; source preservation alone never established it. |

CA-03's audited implicit tuning block includes the NetAdapter buffer feature,
DocumentPolicy crash stack configuration, GPU-channel establishment, compositor
memory and native occlusion tuning, CoreText font matching, browser zoom,
xdg-portal version, active WebGL context count and non-macOS global-shortcut portal
selection. The precise switch strings remain in `engine.py`; extending this list
requires a new audit, not a generic “ignore unknown switches” rule.

## Package source adapters

All six are opt-in compiler flags. They retain ordinary Node/Electron package paths
and require the exact package/input hashes before selecting genuine Mite services.
These are application dependency ports, not a Node/NAPI compatibility layer. Their
existence still means this is an adapted application.

| TODO / status | Exact dependency / owned adapter | Why; replacement and removal validation |
| --- | --- | --- |
| CA-19 — Port | `@vscode/policy-watcher` 1.7.0; `tooling/code_oss/policy-watcher.mjs`; `--mite-policy-watcher` | Select `mite:policy-watcher` instead of the Node addon loader. It reads actual managed preferences and owns notifications; no fabricated empty policy snapshot. Replace with an upstream alternate-runtime loader or genuine compatible addon boundary. Restore the loader and run unchanged NativePolicyService/NativeManagedSettingsService, including actual reads, updates, removals and disposal. |
| CA-20 — Port | `native-keymap` 3.3.9; `native-keymap.mjs`; `--mite-native-keymap` | Select genuine Carbon keyboard layout/key mapping and change observation. Replace the addon dependency with upstream capability-based loading or a real compatible native boundary. Restore the package loader and validate actual layout/map/change events, platform failures and teardown. |
| CA-21 — Port | `@vscode/sqlite3` 5.1.12-vscode; `sqlite.mjs`; `--mite-sqlite` | Select `mite:sqlite`, backed by actual SQLite persistence, rather than NAPI. Replace through upstream alternate-runtime loading or a genuine compatible addon boundary. Restore the loader; run unchanged storage consumers against real database creation, transactions, readback and close/error paths. |
| CA-22 — Port | `@vscode/deviceid` 0.1.5; `deviceid.mjs`; `--mite-deviceid` | In `devdeviceid.js`, use real asynchronous uuid ESM import at an already asynchronous generation point because synchronous require(ESM) is unavailable. In `storage.js`, use real `node:fs` and recursive `mkdir` instead of fs-extra's eager dependency graph. IDs remain genuinely persisted. Replace with correct synchronous ESM loading and the needed filesystem contracts, or upstream async/runtime-neutral imports. Restore both files and prove stable identity across fresh processes and actual storage errors; never substitute temporary IDs. |
| CA-23 — Port / optional-feature deferrals | `undici` 7.29.1; `undici.mjs`; `--mite-undici` | Three audited files: `lib/core/util.js` leaves unavailable Node version metadata undefined rather than inventing a version; `lib/util/runtime-features.js` uses genuine builtin availability to report unsupported optional `markAsUncloneable`; `lib/web/webidl/index.js` makes only the absent ambient MessagePort trait false on Mite. All other required globals remain strict. The complete runtime JS audit establishes the version fields' unused status, not full Undici support. Replace with real worker/MessagePort facilities and upstream runtime-neutral feature/version detection. Restore each file separately and run original transport/WebIDL/clone tests; never add a fake class for a type assertion. |
| CA-24 — Port | `@vscode/fs-copyfile` 2.0.0; `fs-copyfile.mjs`; `--mite-fs-copyfile` | `native.js` selects real libuv file-copy/clone operations and native macOS volume clone capability; `cp.js` selects the actual bounded filesystem copier. No whole-directory native clone claim. Replace with upstream alternate-runtime loading or an actually compatible addon contract. Restore both files and verify file clone force/failure, directory copy, symlinks, errors and actual volume/platform capability. Other native platforms remain explicit until verified. |

Package provenance records original/output/package/adapter hashes. Undici also
records the audited unchanged runtime files. `package-adaptations.mjs` verifies
retained adapters during additive refresh. `migrate-undici.mjs` is a narrowly
verified rollback/reapply tool for known earlier Undici records; it is not
permission to rewrite arbitrary stage metadata. These mechanisms must survive
until the corresponding source modifications are removed.

## Build, resources and diagnostic profile

These changes must also stay visible, but they are not replacements for missing
runtime behavior.

| TODO / status | Mechanism and reason | Exit or retention criterion |
| --- | --- | --- |
| CA-25 — Build | `compile.mjs` transpiles pinned actual TS/JS/JSON with the pinned build tool, preserves source imports/licenses and records input/output hashes. Prepared dependencies are verified; native install scripts are not silently treated as implemented addons. | Retain as reproducible build tooling, or replace with an upstream-supported Firefly build that preserves equivalent provenance. Verify original source inputs and actual runtime imports; successful compilation is not editor acceptance. |
| CA-26 — Build | `nls.mjs` extracts actual upstream NLS keys/English strings and emits the real catalog. `renderer-assets.mjs` stages actual CSS/fonts/resources and import-map/resource inputs. | Retain or use upstream-produced equivalent assets. Compare original keys/catalog/assets and load real workbench resources; no empty catalogs or placeholder styles. |
| CA-27 — Build adaptation | `css_development.py`, `css-development.ts`: replace development ripgrep CSS discovery with the compiler's actual staged CSS inventory. | Restore original discovery once its genuine executable/process/file contract is available, or use the supported built asset path. Verify the same real CSS entries, ordering, edits and missing-file failures. |
| CA-28 — Diagnostic profile | `probe-profile.mjs` consumes explicit `--native-title-bar`, creates a fresh owned private profile and extension directory, and writes supported `window.titleBarStyle: native` settings. It respects the option terminator and never overwrites personal settings/extensions. | Keep as an explicit diagnostic choice if useful; remove any need for it from default acceptance once the original caption/workbench path works. Verify normal invocation remains unchanged and the default profile is tested separately. A native-caption profile cannot establish custom-titlebar acceptance. |

The overlay is applied to a separate build tree; the pinned checkout and Ladybird
remain unmodified. Electron-to-Firefly host selection is explicit integration, not
an Electron version impersonation. Native framework fixes such as real IPC,
filesystem, crypto, contextBridge result copying and actual window behavior belong
in Mite/Firefly, not in expanding this overlay indefinitely.

## Tracked removal checklist

These checkboxes remain open even where a genuine native adapter is implemented.
Closing one requires the original-path validation specified in its inventory row.
Build/diagnostic items may instead be retained explicitly after review.

- [ ] CA-01: upstream truthful native-runtime detection.
- [ ] CA-02: replace disabled Node/V8 cache imports and cache-path omission.
- [ ] CA-03: replace each omitted implicit engine-tuning switch.
- [ ] CA-04: remove disabled `v8CacheOptions` omission.
- [ ] CA-05: replace script-paste omission with genuine paste semantics.
- [ ] CA-06: replace TouchBar initialization/update deferral.
- [ ] CA-07: retire or explicitly retain offscreen-only first-mouse diagnostic omission.
- [ ] CA-08: restore webview creation, protocol, find and shortcut integrations.
- [ ] CA-09: restore remote resolvers, URI callbacks, loader channel and main handlers.
- [ ] CA-10: restore remote CLI inputs and each omitted scheme privilege.
- [ ] CA-11: restore display-capture registration, recording and permission-settings paths.
- [ ] CA-12: restore updater integration or upstream its genuine disabled branch.
- [ ] CA-13: restore genuine utility authentication forwarding.
- [ ] CA-14: resolve unsigned-library policy without weakening native security.
- [ ] CA-15: remove implicit shared-process inspector deferral.
- [ ] CA-16: remove implicit agent-host inspector deferral.
- [ ] CA-17: remove extension-host inspector, network-inspection and DNS-order omissions.
- [x] CA-18: remove the lazy diff-worker source helper/transform; keep the original eager import and constructor.
- [x] CA-18-V: run the unchanged diff-worker count/detail/hunk handlers and disposal against genuine workers.
- [ ] CA-18-I: accept actual Workbench diff-service invocation through its normal application lifecycle.
- [ ] CA-19: remove policy-watcher package loader modification.
- [ ] CA-20: remove native-keymap package loader modification.
- [ ] CA-21: remove sqlite package loader modification.
- [ ] CA-22: restore deviceid UUID loading and storage dependency paths.
- [ ] CA-23: restore Undici version, optional worker trait and MessagePort trait paths.
- [ ] CA-24: restore fs-copyfile native loader and cp paths.
- [ ] CA-25: review continued use of the owned compiler/dependency build path.
- [ ] CA-26: review continued use of owned NLS/resource staging against original assets.
- [ ] CA-27: restore CSS discovery or accept an upstream built-resource path.
- [ ] CA-28: validate default captions separately; retain native-titlebar profile only explicitly.

## Replacement order

1. Replace active runtime blockers in Mite/Firefly first. Re-run the unchanged
   consumer before adding another source exception.
2. Verify the unchanged CA-18 diff consumer against the implemented workers,
   and prioritize CA-15–17 genuine debug/process semantics over accumulating
   more lazy imports or flag omissions. Keep unavailable capabilities explicit
   while these are open.
3. Retire package loader ports through an upstream alternate-runtime contract or a
   genuine compatible native interface. Do not implement Node/V8 identity or load
   an incompatible binary merely to erase a manifest entry.
4. Restore deferred user features individually: paste, remote resources, webviews,
   display capture, updates and TouchBar each need their own real acceptance.
5. Only claim unchanged Code-OSS acceptance when the required overlays are gone or
   accepted upstream, the original application paths run, and actual open/edit/save
   and extension behavior pass. Current startup milestones do not meet that gate.

## Retiring an existing CA-18 diagnostic stage

A fresh overlay no longer contains either the diff service replacement or its
owned helper. Existing stages must retire only this known ownership before normal
refresh; the generic orphan-output and package-preservation guards stay strict.
The stage owner performs this migration after stopping its application processes.
`node tooling/code_oss/retire-diff-worker.mjs STAGE` verifies and reports the plan
without writes. Add `--apply` only for the narrow stage-helper retirement; the
canonical overlay update and full compiler refresh remain separate owner actions.
The tool retains an original manifest backup and a retirement journal.

1. Verify the complete existing canonical overlay and `verifiedRefresh(stage)`;
   verify all six package adapters and their sidecars/audits as usual. Preserve
   backups of both original completion manifests. Refuse symlinks, edited bytes,
   unknown ownership or any additional removed output.
2. Require the old-to-new overlay path difference to be exactly
   `src/vs/platform/agentHost/node/diffComputeService.ts` and
   `src/vs/base/common/elytraDiffWorker.ts`. Their old output SHA-256 values must
   respectively be `e1073d3dfb6289e0c3b4851a130040f49f5d2454e355d47e2719a27cae6bf1a7`
   and `c7062426dc4b286a0f49975c35c3d1682b8d6347514034b747f8ab0d636cf6bf`.
3. Require exactly one old stage input for that helper, with the latter source
   hash and output `src/vs/base/common/elytraDiffWorker.js`, whose SHA-256 is
   `ec3c7d0c4867646e4e94653cd10bf44aed893a9a382556bdfcc35529ca238cab`.
   Retire only this verified compiled helper and its input record. Leave the old
   diff-service input/output record in place for normal verified replacement.
   Make the old completion manifest unavailable before mutation; publish the
   narrowly revised intermediate record only after that one removal succeeds.
4. Run the canonical compiler `--refresh` with the fresh overlay and unchanged
   package selections. It recompiles the diff service from the original pinned
   source (`d33579abe20a405fdc4737d3e7ef2367eadbc41b7d5854b5fe30f77a78bf74de`).
   Preserve all six package-adaptation records byte-for-byte. Do not bypass checks
   by rewriting the entire stage's provenance or adopting unrecorded files.
5. Retire those two verified files from the canonical overlay, copy the fresh
   overlay/manifest and reverify both ownership sets. Confirm the retired helper
   is absent and the compiled original diff service imports the real Worker.
   Original worker operations now pass CA-18-V; actual Workbench integration remains CA-18-I.
