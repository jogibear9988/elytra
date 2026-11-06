# Notifications

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Requires OS notification permission. Nothing is posted during automated checks. Firefly Notification is absent.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/notifications/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/notifications/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- `Notification.isSupported` (not a supported Firefly desktop export)
- `Notification.show` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/notifications/index.html)
- [main.mjs](../../examples/electron/notifications/main.mjs)
- [renderer.mjs](../../examples/electron/notifications/renderer.mjs)

## Operation and validation

Click Show notification and grant OS permission if desired. Validate appearance, focus/click behavior and OS notification preferences manually. Merely constructing Notification does not prove display; tests do not post system notifications.

## Reference and remaining work

[Electron’s Notifications guide](https://www.electronjs.org/docs/latest/tutorial/notifications) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
