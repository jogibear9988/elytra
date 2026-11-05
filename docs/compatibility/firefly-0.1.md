# Firefly 0.1 development compatibility

Public Firefly APIs are not implemented yet. No Electron migration fixture
currently passes. A separate native macOS ARM64 headless LibWebView probe renders
HTML/CSS, executes LibJS DOM code and observes isolated helper processes. This is
an internal embedding gate. A native macOS Qt window gate also passes input,
resize, navigation, close and renderer-crash tests, using the upstream presentation
widget. Mite main-process integration and BrowserWindow support remain unsupported;
native Windows/Linux and full sandbox validation remain outstanding.

| Surface | Status | Acceptance gate |
| --- | --- | --- |
| `app`, lifecycle | unsupported | Real Mite main process and lifecycle tests |
| `BrowserWindow`, `webContents`, loadFile/loadURL | unsupported | Native window plus Ladybird WebContent process proof of concept |
| `ipcMain`, `ipcRenderer` | unsupported | Validated, bounded transport and malicious-input tests |
| preload, `contextBridge` | unsupported | Isolated realm and no renderer access to main-process capabilities |
| dialog, Menu, MenuItem, Tray | unsupported | Native services and observable Electron fixture behavior |
| clipboard, shell, screen, nativeImage, session | unsupported | Native services and migration fixtures |
| Chromium/V8 APIs, Chrome extensions | unsupported | Outside the initial scope |
| Packaging on Windows/macOS/Linux | unsupported | Native build, installation and launch smoke tests |

The intended import is `firefly` in place of `electron`; it is not usable yet.
