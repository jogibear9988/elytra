# Offscreen rendering

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Real offscreen paint events, not screenshots relabeled as offscreen rendering. Firefly offscreen/paint API is absent; no frame-rate performance claim.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/offscreen-rendering/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/offscreen-rendering/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- [app.disableHardwareAcceleration](../api/app.md)
- [webContents.setFrameRate](../api/web-contents.md)
- `paint` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/offscreen-rendering/index.html)
- [main.mjs](../../examples/electron/offscreen-rendering/main.mjs)
- [renderer.mjs](../../examples/electron/offscreen-rendering/renderer.mjs)

## Operation and validation

The native paint callback reports the first bitmap byte count and dirty rectangle. The sample uses offscreen:true and a fixed 30-fps request. No screenshot file is written and no frame-rate/performance certification is inferred. This hidden-window reference stays running; terminate it from the launching terminal after observing the paint report.

## Reference and remaining work

[Electron’s Offscreen rendering guide](https://www.electronjs.org/docs/latest/tutorial/offscreen-rendering) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
