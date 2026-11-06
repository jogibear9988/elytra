# Window customization

**Firefly:** Partial native macOS ARM64 slice; shared startup/assertions pass.

Frameless/drag regions use the actual native adapter. This smoke checks construction/page startup; drag gestures and HTML native min/max controls have separate existing Designer/native tests. Title-bar overlay/transparency/fullscreen are not implied.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/window-customization/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/window-customization/main.mjs
```

The automated path adds --smoke. It checks the stated slice and closes the app;
it does not certify untested native interactions. See the catalog's coverage table.

## API connections

- `BrowserWindow(frame:false)` (not a supported Firefly desktop export)
- `app-region CSS` (not a supported Firefly desktop export)
- `window.close` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/window-customization/index.html)
- [main.mjs](../../examples/electron/window-customization/main.mjs)
- [renderer.mjs](../../examples/electron/window-customization/renderer.mjs)

## Operation and validation

The page is frameless, uses app-region/-webkit-app-region CSS and a browser close button. Native drag regions currently need the documented inline-style allowance for the owned alias shim. Smoke validates startup only; the existing Designer/native chrome gates separately cover min/max states and toolbar controls.

## Reference and remaining work

[Electron’s Window customization guide](https://www.electronjs.org/docs/latest/tutorial/window-customization) is the API-design baseline. This page documents our own code and measured limits. Broader edge cases and non-macOS Firefly desktop validation remain open.
