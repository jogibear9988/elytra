# Navigation history

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Uses the current navigationHistory object, not deprecated WebContents goBack aliases. Firefly host history APIs and IPC remain absent.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/navigation-history/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/navigation-history/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- [webContents.navigationHistory](../api/web-contents.md)

## Source files

- [index.html](../../examples/electron/navigation-history/index.html)
- [main.mjs](../../examples/electron/navigation-history/main.mjs)
- [renderer.mjs](../../examples/electron/navigation-history/renderer.mjs)
- [second.html](../../examples/electron/navigation-history/second.html)

## Operation and validation

Follow the link to the second local page and use Back/Forward. Main calls the current webContents.navigationHistory object and checks whether movement is possible. Both allowed local documents are authorized IPC senders; arbitrary URLs and subframes are rejected.

## Reference and remaining work

[Electron’s Navigation history guide](https://www.electronjs.org/docs/latest/tutorial/navigation-history) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
