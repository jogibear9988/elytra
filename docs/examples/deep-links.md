# Deep links

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

No protocol registration occurs without the explicit environment opt-in. Packaging needs OS protocol declarations. OS delivery and single-instance handling are unsupported in Firefly.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/deep-links/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/deep-links/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- [app.setAsDefaultProtocolClient](../api/app.md)
- [app.requestSingleInstanceLock](../api/app.md)
- [app.on(open-url)](../api/app.md)

## Source files

- [index.html](../../examples/electron/deep-links/index.html)
- [main.mjs](../../examples/electron/deep-links/main.mjs)
- [renderer.mjs](../../examples/electron/deep-links/renderer.mjs)

## Operation and validation

Set ELYTRA_REGISTER_PROTOCOL=1 only when intentionally registering elytra-example. Install the matching OS package/protocol declarations, then launch an elytra-example:// URL while the app is stopped and while it is running. Validate open-url/second-instance delivery and single-instance ownership on each OS.

## Reference and remaining work

[Electron’s Deep links guide](https://www.electronjs.org/docs/latest/tutorial/launch-app-from-url-in-another-app) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
