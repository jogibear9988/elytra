# Quick start

**Firefly:** Partial native macOS ARM64 slice; shared startup/assertions pass.

Real isolated preload and renderer startup; activation/dock behavior is not implemented.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/quick-start/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/quick-start/main.mjs
```

The automated path adds --smoke. It checks the stated slice and closes the app;
it does not certify untested native interactions. See the catalog's coverage table.

## API connections

- [app.whenReady](../api/app.md)
- [BrowserWindow](../api/browser-window.md)
- [contextBridge.exposeInMainWorld](../api/context-bridge.md)

## Source files

- [index.html](../../examples/electron/quick-start/index.html)
- [main.mjs](../../examples/electron/quick-start/main.mjs)
- [renderer.mjs](../../examples/electron/quick-start/renderer.mjs)

## Operation and validation

The isolated preload exposes only a sample label and add function. The page checks API freezing, the function result and absence of renderer process/require globals before reporting readiness.

## Reference and remaining work

[Electron’s Quick start guide](https://www.electronjs.org/docs/latest/tutorial/examples) is the API-design baseline. This page documents our own code and measured limits. Broader edge cases and non-macOS Firefly desktop validation remain open.
