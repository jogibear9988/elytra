# BrowserWindow

Availability: Firefly main process; partial native macOS ARM64 implementation.
Each window owns a [webContents](web-contents.md) EventEmitter for loading/content
observations. Rendering uses Ladybird, so Chromium web behavior is not guaranteed.

## new BrowserWindow([options])

Construct after app.whenReady. Returns a live BrowserWindow with numeric id and
webContents. options defaults to an empty object; arrays/null/nonobjects reject.

| Option | Accepted value / default |
| --- | --- |
| width | Integer 1..16384, default 800 |
| height | Integer 1..16384, default 600 |
| title | String, default Firefly |
| show | Boolean, default true |
| frame | Boolean, default true; false removes native title bar and enables custom CSS drag regions |
| webPreferences | Object with only the restricted values below; omitted/null uses defaults |
| webPreferences.contextIsolation | true only |
| webPreferences.nodeIntegration | false only |
| webPreferences.sandbox | true only |

Unknown window/preferences options and unsafe preference values throw
ERR_FIREFLY_UNSUPPORTED_API. This includes preload. Invalid types/dimensions throw
ERR_INVALID_ARG_TYPE or ERR_INVALID_ARG_VALUE. Construction before readiness throws
ERR_FIREFLY_NOT_READY; after app exit it throws ERR_FIREFLY_APP_EXITED.

```js
import {app, BrowserWindow} from 'firefly';
await app.whenReady();
const window = new BrowserWindow({width: 1000, height: 700, show: false});
window.on('ready-to-show', () => window.show());
await window.loadFile('index.html');
app.on('window-all-closed', () => app.quit());
```

ready-to-show is an observed native paint event; exact Electron presentation timing
and single-emission parity are not established. Showing immediately with the default
show:true is also supported.

## Controls and queries

| Signature | Return / behavior |
| --- | --- |
| show(), hide(), focus() | undefined; native control |
| isVisible() | boolean |
| setSize(width, height[, animate = false]) | undefined; dimensions as above; truthy animate rejects as unsupported |
| minimize(), maximize(), unmaximize(), restore() | undefined; native state transitions |
| isMinimized(), isMaximized() | boolean; native state queries |
| getSize() | [width, height] from native window |
| setTitle(title) | undefined; title must be string |
| getTitle() | Native window title string |
| loadFile(path[, options]), loadURL(url[, options]) | Forward to webContents; Promise<void> |
| close() | undefined; synchronous cancellable close request, native closure follows |
| destroy() | undefined; bypass close event; native closure follows |
| isDestroyed() | boolean; safe after destruction |

Most controls/queries throw ERR_FIREFLY_OBJECT_DESTROYED after destruction.
destroy is idempotent for destroyed/already-closing windows. Do not assume that
calling destroy makes isDestroyed true before the native closed notification.
A pending load rejects when the window is closed. Loading has its own
[validation and Promise contract](web-contents.md).

## Window registry

| Static method | Return |
| --- | --- |
| BrowserWindow.getAllWindows() | New array of currently registered windows |
| BrowserWindow.fromId(id) | Registered window or null |
| BrowserWindow.fromWebContents(contents) | Owning registered window or null |

Windows leave the registry on closure. Holding a JS reference does not keep a native
closed window usable. Keep application ownership explicit and stop background work
when its window closes.

## Events

| Event | Arguments / contract |
| --- | --- |
| close | event; synchronous preventDefault cancels normal close |
| closed | No arguments; native closure recorded, registry entry removed |
| resize | No arguments; observed native resize |
| minimize, restore, maximize, unmaximize | No arguments; observed native window-state changes |
| ready-to-show | No arguments; native paint observed |
| page-title-updated | event, string title, boolean explicitSet; preventDefault suppresses automatic window-title assignment |

Current title notifications set explicitSet to true; this is not full Electron
parity. A window title set by the app can differ from webContents.getTitle().
The close event does not implement renderer beforeunload. Event objects expose
preventDefault/defaultPrevented, not the full Electron event structure.

## Custom title bar and dragging

Use frame:false and HTML buttons for a fully custom bar. This is different from
Electron's titleBarStyle:hidden, which retains macOS traffic lights; that option
still rejects here. The current owned adapter recognizes stylesheet declarations:

```css
.toolbar { app-region: drag; -webkit-app-region: drag; user-select: none; }
.toolbar button { app-region: no-drag; -webkit-app-region: no-drag; }
```

The host intercepts left-button drag/double-click in drag rectangles and subtracts
no-drag rectangles so buttons still receive input. Double-click toggles maximize.
Regions update at 250 ms intervals; snapshots older than one second stop intercepting
input. Navigation clears them. This is an experimental macOS adapter, not full
Electron non-client-area behavior or native resize-edge parity.

Supported scope: top-document style elements and same-origin linked stylesheets,
ordinary selectors/media rules and observed rectangular layout. Inline style
attributes, shadow-root/frame styles, @import chains, cross-origin sheets, strict CSP
that blocks the alias stylesheet, live CSSOM-only edits and browser zoom scaling
are not validated/supported. At most 256 matching elements are accepted. Put the
bar in the top document, use same-origin CSS, and allow its style policy. No native
bindings are installed in page JS; the owned shim only supplies bounded geometry.

Minimize/maximize/restore states match the pinned Electron fixture on this host.
Firefly state events pass; Electron maximize-event delivery could not be established
in the locked desktop session, so full reference event timing/presentation remains
unverified. See [ADR 0055](../architecture/0055-custom-window-chrome.md).

## Missing surface and evidence

Menus, fullscreen, bounds/position, parent/modal windows, icons,
platform material/style options and the wider Electron window surface are absent.
Only constructor options listed above are accepted. Public preload/IPC remain
unsupported; see [security](../guides/security.md).

Implementation: [firefly.js](../../firefly/core/firefly.js) and owned native adapter.
Validation: [Firefly runner](../../tooling/test_firefly.py), including controls,
registry, cancellation/destruction and unsupported options against the pinned
Electron reference. Native Windows/Linux remain unverified. See the
[compatibility matrix](../compatibility/firefly-0.1.md) for measured limits.
