# Menus

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Application and context menus use the real Menu API. Firefly Menu/MenuItem are not implemented.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/menus/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/menus/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- `Menu.buildFromTemplate` (not a supported Firefly desktop export)
- `Menu.popup` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/menus/index.html)
- [main.mjs](../../examples/electron/menus/main.mjs)
- [renderer.mjs](../../examples/electron/menus/renderer.mjs)

## Operation and validation

Inspect the application menu, use Report/Inspect, and right-click the page for a native context menu. Validate accelerator dispatch, native focus, roles and teardown on the target OS. DOM menus are not equivalent native-menu acceptance.

## Reference and remaining work

[Electron’s Menus guide](https://www.electronjs.org/docs/latest/tutorial/menus) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
