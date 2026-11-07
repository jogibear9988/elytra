# ADR 0131: Explicitly separate Code-OSS's implicit Chromium defaults

Status: accepted for an opt-in application overlay; not a generic Electron shim.

## Context

After app.commandLine, the actual pinned Code-OSS entry rejects enable-features.
Firefly cannot apply Chromium/Blink/V8 feature flags to LibWeb/LibJS. The upstream
configuration mixes implicit runtime tuning with user settings and application
flags. Silently accepting everything would hide missing behavior, including
security-sensitive sandbox settings.

## Decision

Extend the hash-checked source overlay with explicit --libjs-engine, independent
of --libjs-cache. Change only four unique insertion seams in pinned mainImpl.ts:
import an owned helper, validate user settings before mutations, and wrap the exact
implicit engine-default block in a host branch. Preserve every original byte,
including license, tuning comments, user/app handling and the original sandbox
branch. Reject input hash, missing/duplicate seam or order drift before output.

On actual Mite, skip only audited implicit Chromium/Blink/Electron defaults and
record their names/reasons in frozen process._vscodeLibJSEngineStatus metadata.
The scope says implicit-code-oss-engine-tuning-only and rendererCompatibility says
not-established. No unsupported API is exported, no engine capability is invented,
and no setting is silently claimed to have taken effect. Node/Electron keeps the
original tuning calls; the source branch checks the real versions metadata.

Reject explicit incompatible user options from CLI, argv.json or native switch
presence before upstream setting/application-argv mutations. Include V8 flags,
feature overrides, proxy/rendering/debug/portal requests and sandbox-disabling
options. Respect minimist's canonical sandbox field: its default sandbox:true is
also mirrored into the no-sandbox alias. Check that raw native switch separately
so --no-sandbox=false cannot be confused with absence. Keep application-owned
logging/proposed-API/in-memory-secret-storage handling unchanged.

## Audit of implicit defaults

| Omitted upstream tuning | Reason / remaining behavior |
| --- | --- |
| NetAdapterMaxBufSizeFeature | Chromium NetToMojo buffer sizing does not configure Mite networking |
| DocumentPolicyIncludeJSCallStacksInCrashReports | Chromium stack-collection feature unavailable; native crash reports are separate |
| EarlyEstablishGpuChannel, EstablishGpuChannelAsync | Chromium GPU-channel startup is not a LibWeb option |
| UpdateCompositorMemoryPolicyOnScreenChange | Chromium compositor memory policy does not configure Ladybird's compositor |
| GlobalShortcutsPortal (Linux) | Electron Wayland global shortcuts remain unimplemented |
| CalculateNativeWinOcclusion | Chromium native-window occlusion implementation is not used |
| FontMatchingCTMigration | Blink font-matching migration is not a LibWeb setting |
| StandardizedBrowserZoom | Blink zoom/layout switch unavailable; broader zoom compatibility remains required |
| xdg-portal-required-version | Electron-specific portal-version selection is unavailable |
| max-active-webgl-contexts | Chromium quota does not configure LibWeb; terminal/WebGL acceptance remains required |

The audit is based on the pinned upstream block and its comments. Omitting a tuning
switch does not prove functional equivalence: actual workbench, terminals, fonts,
zoom, native services and renderer performance remain separate acceptance gates.

## Evidence and consequences

Tests reconstruct the exact original bytes after removing only declared insertions,
reject source/license/unique-seam/overlap drift and compare the actual original vs
adapted configuration function on Node. On actual Mite they prove no implicit
engine calls, unchanged main application argv changes, immutable scoped metadata
and explicit user/sandbox refusal before mutations. The full actual source tree
compiles with its real frozen root dependencies. The real Firefly main entry then
fails at missing app.enableSandbox, preserving exit 1. That original branch is
intentionally not bypassed; no sandbox or workbench success is claimed.

All source/helper/compiled hashes remain reviewable in ELYTRA-ADAPTATIONS.json and
ELYTRA-COMPILE.json. No Ladybird change, Node/V8 fallback, window test or production
packaging claim is involved. Fresh platform acceptance and actual runtime policy
implementation remain required. See the [Code-OSS guide](../guides/code-oss-adaptation.md).
