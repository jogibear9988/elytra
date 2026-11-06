# Dark mode

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Firefly lacks nativeTheme and public IPC. CSS-only theming does not establish native theme compatibility.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/dark-mode/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/dark-mode/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- `nativeTheme` (not a supported Firefly desktop export)
- `ipcMain.handle` (not a supported Firefly desktop export)
- `ipcRenderer.invoke` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/dark-mode/index.html)
- [main.mjs](../../examples/electron/dark-mode/main.mjs)
- [renderer.mjs](../../examples/electron/dark-mode/renderer.mjs)

## Operation and validation

Toggle changes nativeTheme.themeSource; System restores automatic selection. The page uses CSS light-dark colors. Verify both native theme state and visible content after clicking each control; CSS color changes alone do not prove native API support.

## Reference and remaining work

[Electron’s Dark mode guide](https://www.electronjs.org/docs/latest/tutorial/dark-mode) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
