# Progress bars

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Native taskbar/dock progress, not an HTML progress-element substitute. Firefly setProgressBar is absent.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/progress-bars/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/progress-bars/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- [BrowserWindow.setProgressBar](../api/browser-window.md)

## Source files

- [index.html](../../examples/electron/progress-bars/index.html)
- [main.mjs](../../examples/electron/progress-bars/main.mjs)
- [renderer.mjs](../../examples/electron/progress-bars/renderer.mjs)

## Operation and validation

Watch native dock/taskbar progress advance, then clear to -1. Closing the window clears the timer. Verify the native indicator on each target OS; an HTML progress element is not the demonstrated API.

## Reference and remaining work

[Electron’s Progress bars guide](https://www.electronjs.org/docs/latest/tutorial/progress-bar) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
