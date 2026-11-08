# Notifications

**Firefly: partial, manual-only example; actual OS delivery remains unverified.**
The main-process Notification API and macOS UserNotifications provider are
implemented. JavaScript policy and controlled native state/dispatcher tests pass,
but those tests do not post system notifications. This example has no accepted
platforms and remains excluded from feature smoke tests.

For an actual macOS delivery attempt, use an eligible, properly signed `.app`
bundle with stable application identity and OS notification permission. The bare
Firefly command-line executable intentionally reports `Notification.isSupported()`
as false. Its example button rejects before constructing or submitting a native
notification. Linux/Windows notification providers remain unavailable.

## Build and stage

Build the [example catalog](index.md#build-and-run) into a fresh directory:

```sh
node tooling/build_electron_examples.mjs build/desktop-examples-notifications firefly
```

The staged UI can be opened with:

```sh
build/renderer/bin/firefly build/desktop-examples-notifications/notifications/main.mjs
```

This raw CLI launch is a staging/reference check, **not native notification
acceptance**. Follow the [packaging guide](../guides/packaging.md) when preparing an
eligible application bundle. Packaging alone does not verify OS delivery.

A separate Electron reference tree can be staged explicitly:

```sh
node tooling/build_electron_examples.mjs build/desktop-examples-notifications-reference electron
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-notifications-reference/notifications/main.mjs
```

The reference notification action is also manual and unverified. No Electron
Notification support probe, permission request or delivery operation was run for
this slice.

## Request outcome

Clicking **Show notification** checks support, installs `show` and `failed`
listeners, then calls `show()`. No notification is posted during startup or
`--smoke`. A successful action resolves only after native acknowledgement with:

```js
{ nativeRequestAccepted: true, osVisibility: 'unverified' }
```

The page displays that distinction explicitly. Firefly's `show` event acknowledges
acceptance of the OS request; it does not prove that a banner appeared. A `failed`
event, explicit close before acknowledgement or synchronous submission error
rejects the action. The wait is bounded to ten seconds; timeout closes the owned
request. One request can wait at a time, and another click retires the previous
accepted notification. Closing the example window also requests cleanup.

Native removal is best effort and cannot roll back an already displayed alert.
OS permission flows, visible delivery, clicks/action buttons and notification
preferences still require explicit manual acceptance. No feature success is
inferred from constructing an object, starting the app or building the catalog.

## API connections

- [`Notification.isSupported()`](../api/notification.md#static-method-and-construction)
- [`Notification.show()` / `close()`](../api/notification.md#show-and-close)
- [`show` and `failed` events](../api/notification.md#events)
- [Implemented scope and validation](../api/notification.md#platform-and-validation-scope)

## Source files

- [index.html](../../examples/electron/notifications/index.html)
- [main.mjs](../../examples/electron/notifications/main.mjs)
- [renderer.mjs](../../examples/electron/notifications/renderer.mjs)

[Electron’s Notifications guide](https://www.electronjs.org/docs/latest/tutorial/notifications)
is the reference for the broader platform integration. This example remains
`partial`, `smoke: false`, with no platform feature-acceptance claim.
