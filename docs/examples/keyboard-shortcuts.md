# Keyboard shortcuts

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Native menu/global shortcut registration is manual and scoped to this process. Native Firefly shortcut APIs are absent.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/keyboard-shortcuts/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/keyboard-shortcuts/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- `Menu.buildFromTemplate` (not a supported Firefly desktop export)
- `globalShortcut.register` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/keyboard-shortcuts/index.html)
- [main.mjs](../../examples/electron/keyboard-shortcuts/main.mjs)
- [renderer.mjs](../../examples/electron/keyboard-shortcuts/renderer.mjs)

## Operation and validation

Use the Example menu or the registered accelerator; inspect LOCAL_SHORTCUT/GLOBAL_SHORTCUT messages. Registration can fail if another app owns the combination. Cleanup unregisters this process’s shortcuts on will-quit; OS-delivery behavior requires manual testing.

## Reference and remaining work

[Electron’s Keyboard shortcuts guide](https://www.electronjs.org/docs/latest/tutorial/keyboard-shortcuts) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
