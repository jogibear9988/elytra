# Desktop launcher actions

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Linux manual packaging example. The .desktop template must be installed with a real absolute executable path; Firefly Linux desktop packaging remains unavailable.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/desktop-launcher-actions/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/desktop-launcher-actions/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- `process.argv` (not a supported Firefly desktop export)
- `Linux .desktop Actions` (not a supported Firefly desktop export)

## Source files

- [elytra-example.desktop](../../examples/electron/desktop-launcher-actions/elytra-example.desktop)
- [index.html](../../examples/electron/desktop-launcher-actions/index.html)
- [main.mjs](../../examples/electron/desktop-launcher-actions/main.mjs)
- [renderer.mjs](../../examples/electron/desktop-launcher-actions/renderer.mjs)

## Operation and validation

Replace every /ABSOLUTE/PATH template value with the installed runtime/sample location and register the .desktop entry through your Linux packaging workflow. The New window action supplies --new-window; main reads process.argv. No desktop entry is installed automatically.

## Reference and remaining work

[Electron’s Desktop launcher actions guide](https://www.electronjs.org/docs/latest/tutorial/linux-desktop-actions) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
