# Device access

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Manual HID selection with a cancel-first native chooser, scoped file-origin permissions and explicit user gesture. Bluetooth/serial/USB need separate scoped permissions; none is claimed for Firefly.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/device-access/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/device-access/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- `session.setPermissionCheckHandler` (not a supported Firefly desktop export)
- `session.setDevicePermissionHandler` (not a supported Firefly desktop export)
- `navigator.hid` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/device-access/index.html)
- [main.mjs](../../examples/electron/device-access/main.mjs)
- [renderer.mjs](../../examples/electron/device-access/renderer.mjs)

## Operation and validation

Click Choose HID device, then choose or cancel in the native list. Permission checks are restricted to this window and its generated loopback origin. Selection is explicit; device disconnects and Bluetooth/USB/serial integrations require their own tests and scoped permission policies.

## Reference and remaining work

[Electron’s Device access guide](https://www.electronjs.org/docs/latest/tutorial/devices) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
