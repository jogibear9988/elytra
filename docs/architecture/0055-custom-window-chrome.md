# 0055: Custom window chrome without an upstream patch

## Decision

Keep Electron's frame:false and BrowserWindow minimize/maximize/unmaximize/restore
names. Implement them in the owned Qt window adapter. Keep titleBarStyle:hidden
unsupported: removing the whole frame is not the macOS traffic-light variant.

LibWeb at the current pin does not implement Electron's app-region property. The
owned AppRegions.js adapter maps app-region/-webkit-app-region declarations to a
private CSS property in companion style elements, then lets LibWeb resolve selectors,
cascade, media queries and layout. The host obtains bounded JSON rectangles using
existing upstream view evaluation plumbing. No native function, filesystem access,
Mite object or general IPC bridge is installed in page JS.

## Input and lifetime

ChromeWindow intercepts native left mouse input only in drag rectangles minus all
no-drag rectangles. Movement stays in native code; double-click toggles maximize.
A 250 ms poll has one pending result, navigation generation validation, a 64 KiB
text limit, at most 256 rectangles, integer/range validation and viewport clipping.
Snapshots expire after one second, so a blocked/crashed page cannot retain stale
input interception indefinitely. Navigation immediately clears old regions.

Page-controlled geometry is cosmetic authority over this window only. It must never
be used for filesystem/IPC authorization. This adapter does not complete public
preload/contextBridge/ipcMain/ipcRenderer work or a sandbox security audit.

## Scope and limitations

Top-document style elements and same-origin linked stylesheets are supported;
inline attributes, shadow/frame styles, @import chains, live CSSOM-only edits,
cross-origin sheets, strict CSP and browser zoom scaling remain unsupported or
unverified. Region rules belong in an app-owned top-document stylesheet. Full
Electron non-client hit testing, accessibility, edge resizing, fullscreen/platform
style parity and native compositor presentation still require work. Native Windows
and Linux are not verified. There is no downstream Ladybird patch.

## Evidence

The native ChromeProbe passes frameless flags, drag/no-drag native mouse routing,
actual window position deltas, double-click maximize/restore and stale expiration.
The shared Electron/Firefly fixture passes minimize/maximize/restore state changes
and destroyed-object rejection. Firefly state events pass; the locked-host Electron
reference did not deliver a maximize event, so its event timing is not certified.

The actual Firefly/LibWeb regions fixture passes both CSS spellings, geometry,
no-drag rectangles, media rules, dynamic style replacement/removal and absent Node
globals. The full existing Firefly differential/error/crash/quit suite still passes.
Primary design references: [Electron interactions](https://www.electronjs.org/docs/latest/tutorial/custom-window-interactions)
and [custom title bars](https://www.electronjs.org/docs/latest/tutorial/custom-title-bar).
