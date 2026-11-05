# Firefly 0.1 development compatibility

The first Mite-driven native macOS ARM64 Firefly slice is implemented. One shared
Electron 45.2.3 migration fixture passes in both runtimes with the module name
selected at import time. This does not establish general Electron compatibility.
Windows/Linux, preload/IPC isolation and distribution remain outstanding.

| Surface | Status | Evidence / remaining acceptance gate |
| --- | --- | --- |
| `app`, lifecycle | partial | isReady/whenReady, ready, window-all-closed, before-quit, will-quit, quit and explicit quit match the shared fixture; OS activation/dock/Cmd-Q, repeated/cancelled quit flows need coverage |
| `BrowserWindow` | partial | Native macOS Qt/IOSurface view; width/height/title/show, show/hide/focus, size/title controls, registry, cancellable close and destroy; most options, beforeunload and native OS event parity remain incomplete |
| `webContents`, loadFile/loadURL | partial | Shared object, URL/title/PID access, did-finish-load and load promise; local files and an actual Mite HTTP server tested; file/connect errors and renderer-crash event tested; redirects, fragment navigation, overlapping loads and full error shapes need work |
| Renderer capability defaults | partial | No process/require/Buffer/Mite globals, node:fs import denied in shared HTML fixture; this is not a full sandbox/IPC security audit |
| `ipcMain`, `ipcRenderer` | unsupported | Validated, bounded transport and malicious-input tests |
| preload, `contextBridge` | unsupported | Isolated preload realm and constrained main/renderer bridge; preload is explicitly rejected |
| dialog, Menu, MenuItem, Tray | unsupported | Native services and observable Electron fixture behavior |
| clipboard, shell, screen, nativeImage, session | unsupported | Native services and migration fixtures |
| Chromium/V8 APIs, Chrome extensions | unsupported | Outside the initial scope |
| Packaging on Windows/macOS/Linux | unsupported | Native installation/signing/launch smoke tests; current helpers/resources come from the development Ladybird bundle |

Use `require('firefly')` or ESM imports from `firefly` in the main process. There
is no `electron` alias. Standalone Mite does not register or depend on Firefly.
Only width, height, title, show and the secure default webPreferences settings are
accepted initially. Unknown options, preload, nodeIntegration=true,
contextIsolation=false or sandbox=false fail explicitly. Dimensions are restricted
to integer values in 1..16384. Extra load options are not implemented. The
contextIsolation setting does not imply an implemented preload isolated world.

Named ESM exports are fixed snapshots of app and BrowserWindow; CJS and ESM share
the same objects. Relative loadFile paths use the application entry directory.
No BrowserWindow creation is allowed before app is ready. The GUI driver currently
polls Mite's nonblocking libuv pump every millisecond; idle-efficient backend
notifier integration is still required. Main callbacks enter the Mite checkpoint
boundary, and reentrant GUI pumps are deferred.

Native screenshot/input proof from the preceding window gate remains recorded in
PROGRESS.md. In the latest combined build, the screenshot gate could not complete
because macOS reports the desktop locked. Functional Firefly, Electron reference,
ESM/HTTP and headless tests pass, but current visible presentation must be rechecked
after unlocking. No replacement image or fake screenshot pass is used.
