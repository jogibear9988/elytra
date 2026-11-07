# Desktop example catalog

ESM sources for all **21 topics** in the current [Electron examples overview](https://www.electronjs.org/docs/latest/tutorial/examples), plus quick-start and application lifecycle: **23 example apps**. These are original Elytra examples using familiar API names, not a vendored copy of Electron’s guide prose.

The source catalog was reviewed on 2026-11-06. Runtime reference: pinned Electron 45.2.3. All Firefly GUI evidence here is native macOS ARM64. [Alpha 2](https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.2) includes the catalog snapshot at `19ad113`; Alpha 1 predates it. Later master examples need their own build.

## Build and run

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/runtime-js
node tooling/build_electron_examples.mjs build/desktop-examples firefly
build/renderer/bin/firefly build/desktop-examples/quick-start/main.mjs
```

The builder preserves ESM main/page/worker sources and produces sandbox CommonJS preload bundles from ESM. Node/esbuild are build tools. Runtime selection is explicit; there is no Electron/Node fallback if Firefly lacks an API. Build a separate reference tree and launch it with the test-only Electron installation:

```sh
node tooling/build_electron_examples.mjs build/desktop-examples-reference electron
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/quick-start/main.mjs
```

Use a fresh output directory when rebuilding. The shared launcher serves only allowlisted browser assets on a fresh loopback port, with main/utility/preload/config files excluded. Main-process IPC actions, where supported by the chosen runtime, validate the exact window/main frame and approved document URL. The local HTTP server exposes no privileged action route.

## Coverage

**Native shared assertions passed:** quick-start, browser module worker, initial online/offline state, iframe messaging, frameless window startup and lifecycle/cancellation. These narrow checks do not imply complete feature parity.

**Other topics:** Dark mode has partial native API coverage and an explicit manual-validation gate; button/visual acceptance remains open. The other 16 topics are blocked by missing APIs or platform requirements. Their Electron reference implementations are present as code, but hardware, store, OS integration and experimental AI paths remain manual/unverified. A successful preflight rejection is not feature acceptance.

| Example | Firefly status and scope | API dependencies |
| --- | --- | --- |
| [Quick start](quick-start.md) | Partial; native smoke passed | `app.whenReady`, `BrowserWindow`, `contextBridge.exposeInMainWorld` |
| [Dark mode](dark-mode.md) | Partial; direct API/CSS queries passed, buttons/visuals manual | `nativeTheme`, `ipcMain.handle`, `ipcRenderer.invoke` |
| [Device access](device-access.md) | Unsupported; explicit rejection | `session.setPermissionCheckHandler`, `session.setDevicePermissionHandler`, `navigator.hid` |
| [In-app purchases](in-app-purchases.md) | Unsupported; explicit rejection | `inAppPurchase.getProducts`, `inAppPurchase.purchaseProduct` |
| [Keyboard shortcuts](keyboard-shortcuts.md) | Unsupported; explicit rejection | `Menu.buildFromTemplate`, `globalShortcut.register` |
| [Deep links](deep-links.md) | Unsupported; explicit rejection | `app.setAsDefaultProtocolClient`, `app.requestSingleInstanceLock`, `app.on(open-url)` |
| [Desktop launcher actions](desktop-launcher-actions.md) | Unsupported; explicit rejection | `process.argv`, `Linux .desktop Actions` |
| [Local AI handler](local-ai-handler.md) | Unsupported; explicit rejection | `utilityProcess.fork`, `session.registerLocalAIHandler`, `localAIHandler.setPromptAPIHandler`, `LanguageModel` |
| [Menus](menus.md) | Unsupported; explicit rejection | `Menu.buildFromTemplate`, `Menu.popup` |
| [Multithreading](multithreading.md) | Partial; native smoke passed | `Worker`, `postMessage` |
| [Native file drag and drop](native-file-drag-drop.md) | Unsupported; explicit rejection | `webContents.startDrag`, `ipcMain.handle`, `nativeImage` |
| [Navigation history](navigation-history.md) | Unsupported; explicit rejection | `webContents.navigationHistory` |
| [Notifications](notifications.md) | Unsupported; explicit rejection | `Notification.isSupported`, `Notification.show` |
| [Offscreen rendering](offscreen-rendering.md) | Unsupported; explicit rejection | `app.disableHardwareAcceleration`, `webContents.setFrameRate`, `paint` |
| [Online and offline events](online-offline.md) | Partial; native smoke passed | `navigator.onLine`, `online/offline events` |
| [Progress bars](progress-bars.md) | Unsupported; explicit rejection | `BrowserWindow.setProgressBar` |
| [Recent documents](recent-documents.md) | Unsupported; explicit rejection | `app.addRecentDocument`, `app.clearRecentDocuments` |
| [Represented file](represented-file.md) | Unsupported; explicit rejection | `BrowserWindow.setRepresentedFilename`, `BrowserWindow.setDocumentEdited` |
| [Spellchecker](spellchecker.md) | Unsupported; explicit rejection | `webContents.session`, `Menu.buildFromTemplate`, `webContents.replaceMisspelling` |
| [Web embeds](web-embeds.md) | Partial; native smoke passed | `iframe`, `WebContentsView` |
| [Taskbar customization](taskbar-customization.md) | Unsupported; explicit rejection | `BrowserWindow.setOverlayIcon`, `BrowserWindow.setThumbarButtons` |
| [Window customization](window-customization.md) | Partial; native smoke passed | `BrowserWindow(frame:false)`, `app-region CSS`, `window.close` |
| [Application lifecycle](lifecycle.md) | Partial; native smoke passed | `app.isReady`, `app.whenReady`, `app.quit`, `app.exit`, `before-quit`, `will-quit`, `quit` |

## Test commands

```sh
python3 tooling/tests/test_electron_examples.py --firefly build/renderer/bin/firefly --electron build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron --firefly-apps build/desktop-examples --electron-apps build/desktop-examples-reference
```

The release driver runs the same catalog checks. The separate asset-boundary check
uses an independent Python HTTP peer against both Node and Mite:

```sh
python3 tooling/tests/test_example_assets.py --mite build/release-mite/bin/mite
```

It checks allowed HTML/CSS/module routes, HEAD, MIME, Host validation, rejected POST
and rejection of main/preload/utility/config/traversal/query routes.

The runner compares real Firefly/Electron processes for the six supported slices, checks ESM syntax and validates explicit rejection for blocked Firefly topics. It passes --smoke to the manual dark-mode app and requires its explicit manual-validation exit rather than leaving an interactive window waiting. That exit does not certify the example's buttons or appearance. It never performs purchases, registers protocols, changes recent documents, requests device permission or posts notifications. Manual prerequisites are listed on each topic page.

Detailed implemented references: [app](../api/app.md), [BrowserWindow](../api/browser-window.md), [webContents](../api/web-contents.md), [preload/contextBridge](../api/context-bridge.md). Missing desktop services are tracked in the [compatibility matrix](../compatibility/firefly-0.1.md).
