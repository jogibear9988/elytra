# Native desktop source metadata and owner-thread delivery

Status: implemented; OS enumeration acceptance remains open. Policy, controlled
native-dispatcher and no-prompt preflight checks pass. This does not implement
screenshot pixels, media streams or Code-OSS display-media authorization.

## Decision

Use macOS ScreenCaptureKit for actual screen/window source metadata. Keep its
Objective-C++ provider independent of LibJS, Qt widgets and application callbacks.
C++ result records contain bounded native IDs/names/errors only. Screen display IDs
are the same CGDirectDisplayID values used by the existing native screen adapter.
Window IDs are actual CGWindowID values. Raw sources have a zero native-window
registry component; the third ID field is not an owning-process flag.

ScreenCaptureKit is a system framework, not another bundled browser/runtime.
Platforms without this provider fail explicitly. Default tests never enumerate:
even a zero-thumbnail request can ask for OS permission. CGPreflightScreenCaptureAccess
and active display metadata checks are not proof of authorized source enumeration
for a signed packaged app. An opt-in native fixture remains deliberately unrun.

## Thread and lifetime boundaries

The native provider admits at most eight process-wide OS requests. A deadline or
JavaScript cancellation cannot cancel ScreenCaptureKit, so it cannot refund that
OS slot. Only the real OS callback releases it. The provider bounds source count,
UTF-8 names and aggregate metadata, never creates screenshots or SCStream objects,
and reports denied/unavailable/error states without fabricated source success.

The main bridge owns a dispatcher with monotonic request IDs and a weak posting
token. Worker completions contain plain results and weak native state only: no VM,
realm, GC roots or JS functions can be retained or destroyed by an OS callback.
A mutex serializes posting against owner teardown. Qt queues delivery to the owner;
only that thread touches pending IDs and notifications. Shutdown invalidates the
receiver before deleting its QObject context. Late or canceled IDs lose delivery
authority. The existing bridge then handles busy-VM deferral and normal job dispatch.

The JavaScript facade separately bounds eight pending requests and a 30-second
wait, validates complete native snapshots, filters selected source types and
rejects pending promises on exit. IDs and source strings alone grant no renderer
capability. Future display-media selection must bind a fresh source and permission
to an authenticated document/process rather than trusting an application string.

## Empty images and acceptance

Zero-thumbnail sources use a real empty NativeImage representation: no pixel
storage, zero size/encoded bytes and the measured empty-image transforms. Nonempty
image codecs, thumbnails and icons fail explicitly. This is not an image codec
placeholder that silently returns an empty image for arbitrary input.

Shared JS policy tests use controlled metadata; an exact production C++ dispatcher
probe uses controlled native worker completions and checks owner delivery, errors,
quotas, cancellation, ID reuse, stop, receiver deletion and completion after VM
teardown. Neither proves OS source enumeration or capture. Native preflight tests
check only current availability and actual display IDs. The real prepared Code-OSS
ready graph advances past the imports to Notification, without an accepted workbench.

References: [Electron desktop capture](https://www.electronjs.org/docs/latest/api/desktop-capturer),
[Chromium source ID format](https://github.com/chromium/chromium/blob/main/content/public/browser/desktop_media_id.cc),
[public API and opt-in fixture](../api/desktop-capturer.md).
