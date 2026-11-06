# Dark mode

**Firefly:** Partial native macOS implementation. Direct nativeTheme/CSS-query fixtures pass in Firefly and Electron; this example's individual buttons and visible native styling still require manual acceptance.

Firefly now provides [nativeTheme](../api/native-theme.md) and isolated [invoke/handle IPC](../api/ipc.md). Native preferences drive renderer color-scheme queries; CSS-only class toggles are not the implementation.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/dark-mode/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/dark-mode/main.mjs
```

This example remains in manual validation mode. Its startup is not recorded as button/visual acceptance. Read the platform/setup requirements; the direct API/CSS fixture has separate measured coverage.

CI passes --smoke to request a bounded run. This example exits with
ERR_EXAMPLE_MANUAL_VALIDATION after loading its page, rather than waiting for
button interaction. That explicit manual gate is distinct from a missing API;
neither result counts as button/visual acceptance. Normal interactive startup
without --smoke remains available for the manual checks.

## API connections

- [nativeTheme](../api/native-theme.md)
- [ipcMain.handle](../api/ipc.md)
- [ipcRenderer.invoke](../api/ipc.md)

## Source files

- [index.html](../../examples/electron/dark-mode/index.html)
- [main.mjs](../../examples/electron/dark-mode/main.mjs)
- [renderer.mjs](../../examples/electron/dark-mode/renderer.mjs)

## Operation and validation

Toggle changes nativeTheme.themeSource; System restores automatic selection. The page uses CSS light-dark colors. Verify both native theme state and visible content after clicking each control; CSS color changes alone do not prove native API support.

## Reference and remaining work

[Electron’s Dark mode guide](https://www.electronjs.org/docs/latest/tutorial/dark-mode) is the API-design baseline. This page documents our own code and measured limits. Complete button/visible-chrome, physical system-change and further platform acceptance before broadening this topic's partial status.
