# Optional display capture in the Code-OSS local desktop experiment

## Context

After native policy, local IPC and document-bound permissions, the actual pinned
Code-OSS main reaches setDisplayMediaRequestHandler in configureSession. The pinned
renderer lacks getDisplayMedia. A callback-storing Session setter would not provide
capture and would falsely imply native enforcement.

## Decision

Extend the existing explicit application adaptation, not Firefly's public API.
Both the build-time local-desktop overlay and the runtime
ELYTRA_CODE_OSS_LOCAL_DESKTOP=1 flag on the truthful LibJS host are required.
Ordinary Node/Electron retains its original branches, even with that environment
flag. Default/disabled paths retain the original source behavior.

Skip only the original display-media handler registration when this mode is
active. Preserve both session permission handlers and all webRequest filters.
The frozen diagnostic status and startup notice disclose display capture alongside
webviews, remote resources and engine code caches as disabled.

Adapt the unchanged recording consumer at its application-owned entry points:
isSupported returns false; recording, screen-capture authorization and capture
settings attempts fail with ERR_CODE_OSS_LOCAL_DESKTOP_UNSUPPORTED before reaching
capture or OS collaborators. Read-only authorization status remains available.
Do not request permissions, open settings, replace the renderer or modify Ladybird.
This deferral is temporary; real capture remains part of the larger compatibility
goal.

## Provenance and validation

The source revision is 6531648f5ace8b77ae799262bdc97bf048461938. The added
vendor/code-oss-tests/local-recording-electron.ts pin preserves the original license
and exact upstream bytes. Its SHA-256 is
51c0ac06d28d65ea27824531e34aec78211648504c8ae931223427cab4fc3bdf.
The overlay checks its manifest and exact source seams before producing output.

The maintained local-desktop policy test passes on Node and Mite. It verifies
source fingerprints, byte reversal outside declared changes, seam drift and
duplicate rejection. It executes actual registration and recording method bodies
with controlled collaborators: ordinary branches retain original calls; enabled
branches refuse actual recording/authorization/settings before those collaborators.
It verifies the permission handlers and request filters remain unchanged. No OS
capture, prompt or settings launch occurs in these tests.

The fresh build/code-oss-capture-final compiles 11,536 sources. The actual main
entry and app ready pass, with display capture disclosed as disabled. The first
failure is session.defaultSession.webRequest.onBeforeRequest because webRequest is
undefined; Error.captureStackTrace is a secondary reporting failure. Evidence is
build/display-capture-code-oss.log. These observations establish startup progress,
not a workbench or capture implementation.

## Remaining work

Implement genuine authenticated request/frame filtering before accepting the
unchanged security filters. Native frame work is still unaccepted. Preserve the
accepted permission slice and all document/origin ownership guarantees. Native
utility/renderer port handoff, worker/device APIs, full display capture, editor
open/edit/save and native platform acceptance remain separate requirements.
