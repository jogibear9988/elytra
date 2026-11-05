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
| ready-to-show | No arguments; native paint observed |
| page-title-updated | event, string title, boolean explicitSet; preventDefault suppresses automatic window-title assignment |

Current title notifications set explicitSet to true; this is not full Electron
parity. A window title set by the app can differ from webContents.getTitle().
The close event does not implement renderer beforeunload. Event objects expose
preventDefault/defaultPrevented, not the full Electron event structure.

## Missing surface and evidence

Menus, fullscreen/maximize/minimize, bounds/position, parent/modal windows, icons,
platform material/style options and the wider Electron window surface are absent.
Only constructor options listed above are accepted. Public preload/IPC remain
unsupported; see [security](../guides/security.md).

Implementation: [firefly.js](../../firefly/core/firefly.js) and owned native adapter.
Validation: [Firefly runner](../../tooling/test_firefly.py), including controls,
registry, cancellation/destruction and unsupported options against the pinned
Electron reference. Native Windows/Linux remain unverified. See the
[compatibility matrix](../compatibility/firefly-0.1.md) for measured limits.
