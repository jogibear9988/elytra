# Firefly desktop API

Status: partial native macOS ARM64 development implementation. Firefly aims for
Electron-compatible APIs using Mite + Ladybird, not general Electron compatibility
today. Node integration in renderers is unavailable. [Compatibility matrix](../compatibility/firefly-0.1.md)
and [renderer build guide](../development/renderer-build.md) record the acceptance gates.

Detailed contracts: [app](app.md), [BrowserWindow](browser-window.md),
[webContents](web-contents.md), [IPC](ipc.md). Start with the [desktop tutorial](../guides/first-desktop-app.md)
or [Electron migration guide](../migration/from-electron.md).

## Application entry

```js
import {app, BrowserWindow} from 'firefly';
await app.whenReady();
const window = new BrowserWindow({width: 1200, height: 800, title: 'My app'});
await window.loadFile('index.html');
app.on('window-all-closed', () => app.quit());
```

Launch with `firefly main.mjs`; CommonJS `require('firefly')` also works. Mite's
supported Node-style APIs are available in the main process. There is no public
`electron` import alias. A simple migration changes the import from electron to
firefly and checks supported options/events; larger apps need a compatibility review.

## app

app is an EventEmitter.

| Method | Return / behavior |
| --- | --- |
| `app.isReady()` | boolean |
| `app.whenReady()` | Shared Promise resolved at ready |
| `app.quit()` | Request normal quit; cancellable before-quit, window close and will-quit |
| `app.exit([exitCode = 0])` | Force exit without normal close cancellation; signed int32 code |

Events: `ready`, `window-all-closed`, `before-quit(event)`, `will-quit(event)` and
`quit(event, exitCode)`. `event.preventDefault()` cancels before-quit/will-quit.
Nested quit requests coalesce; retry after cancellation is supported. Forced exit
suppresses window-all-closed and normal quit cancellation, destroys windows and emits
closed/quit. A current JS turn can finish before native termination.

Platform activation/dock/Cmd-Q integration, beforeunload handling, paths, application
menus, single-instance locking, packaging metadata and most Electron app methods are
not implemented. Calling an absent method throws an ordinary JS API error.

## BrowserWindow

Construct only after app readiness:

```js
const window = new BrowserWindow({
  width: 1000,
  height: 700,
  title: 'Firefly',
  show: true,
  webPreferences: {
    contextIsolation: true,
    nodeIntegration: false,
    sandbox: true
  }
});
```

Defaults: width 800, height 600, title Firefly, show true, frame true. Dimensions must be integers
in 1..16384. frame:false removes the native title bar; [custom chrome](browser-window.md#custom-title-bar-and-dragging)
uses Electron app-region CSS with documented adapter limits. Only the options
shown, frame and an absolute CommonJS preload path are supported. The three boolean
webPreferences values can only request their safe defaults. nodeIntegration true,
sandbox false, contextIsolation false and other options explicitly reject with
ERR_FIREFLY_UNSUPPORTED_API. See [preload/contextBridge](context-bridge.md) for
source limits, navigation policy and remaining differences.

| Member | Behavior |
| --- | --- |
| `window.id` | Native window identity |
| `window.webContents` | Associated WebContents EventEmitter |
| `window.loadFile(filePath[, options])` | Forward to webContents; Promise<void> |
| `window.loadURL(url[, options])` | Forward to webContents; Promise<void> |
| `window.show()`, `hide()`, `focus()` | Native window controls |
| `window.minimize()`, `maximize()`, `unmaximize()`, `restore()` | Native window-state controls |
| `window.isMinimized()`, `isMaximized()` | Native state booleans |
| `window.isVisible()` | boolean |
| `window.setSize(width, height[, animate = false])` | Resize; animated resizing rejects |
| `window.getSize()` | [width, height] |
| `window.setTitle(title)`, `getTitle()` | Native title control |
| `window.close()` | Cancellable close request |
| `window.destroy()` | Close without the cancellable close event |
| `window.isDestroyed()` | boolean; safe after destruction |
| `BrowserWindow.getAllWindows()` | Array of live windows |
| `BrowserWindow.fromId(id)` | Window or null |
| `BrowserWindow.fromWebContents(contents)` | Window or null |

Events: `close(event)` (preventDefault cancels), `closed`, `resize`, `ready-to-show`
and `page-title-updated(event, title, explicitSet)`. Cancel page-title-updated to
preserve an application-set title. Most methods throw ERR_FIREFLY_OBJECT_DESTROYED
when used after destruction. ready-to-show comes from native painting; full Electron
presentation and event timing parity are not established.

## webContents

| Method | Behavior |
| --- | --- |
| `loadFile(filePath[, options])` | Resolve relative to the entry module's directory; load file URL |
| `loadURL(url[, options])` | file/http/https schemes only; resolve on did-finish-load |
| `getURL()` | Observed navigation URL |
| `getTitle()` | Observed document title |
| `getOSProcessId()` | Native WebContent renderer PID |
| `isDestroyed()` | Follows owning window lifetime |

Loading options must be absent or empty. No query/hash/headers/post-data options are
implemented. Loading starts a 30-second deadline; another load cancels the previous
pending promise with ERR_ABORTED. Window closure rejects its pending load. Invalid
URLs, unsupported schemes, missing files, network failures and renderer crashes
reject. Redirect/fragment/overlap and full error parity remain under development.

Events include `did-start-loading`, `did-finish-load`,
`did-fail-load(event, errorCode, description, validatedURL, isMainFrame)` and
`render-process-gone(event, details)`. Crash details currently use reason crashed
and a simplified exit code. This is not Electron's full WebContents event surface.

`executeJavaScript`, send, openDevTools, session access and general frame APIs are
absent. The main process cannot use a success placeholder to bypass these limits.

## Renderer and security boundary

Renderer documents run LibWeb/LibJS in separate native processes. They do not receive
require, process, Buffer or Mite filesystem capabilities; node:fs imports fail.
Use ordinary browser HTML/CSS/JavaScript for content. Ladybird web-platform coverage
differs from Chromium and must be checked with the actual application.

Public [preload/contextBridge](context-bridge.md) supports bounded CommonJS source
and frozen data/function publication in a separate realm. [IPC](ipc.md) now supports
invoke/handle request/reply with native sender validation and bounded plain data. Native SDK/helper tests exercise selected boundaries; they do not
prove a complete security audit. Do not expose native privileges to page content.

## Errors and unsupported features

Common explicit codes include ERR_FIREFLY_UNSUPPORTED_API,
ERR_FIREFLY_NOT_READY, ERR_FIREFLY_APP_EXITED, ERR_FIREFLY_OBJECT_DESTROYED,
ERR_INVALID_ARG_TYPE, ERR_INVALID_ARG_VALUE, ERR_INVALID_URL, ERR_ABORTED,
ERR_TIMED_OUT and ERR_FAILED. Messages, stack traces and detailed network error
fields are not generally Electron-compatible.

Dialogs, Menu/MenuItem, Tray, clipboard, shell, screen, nativeImage, session,
auto-updates and production signed/notarized packages remain unimplemented.
Experimental macOS bundles and a gated build workflow are described in the
[release guide](../development/releases.md). Native Windows
and Linux desktop support still require adapters and platform-specific validation.
