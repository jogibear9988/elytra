# Recent documents

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

This changes the user’s recent-document list only after an explicit button click. Tests do not clear or modify that list. Firefly native recent-document APIs are absent.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/recent-documents/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/recent-documents/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- [app.addRecentDocument](../api/app.md)
- [app.clearRecentDocuments](../api/app.md)

## Source files

- [index.html](../../examples/electron/recent-documents/index.html)
- [main.mjs](../../examples/electron/recent-documents/main.mjs)
- [note.txt](../../examples/electron/recent-documents/note.txt)
- [renderer.mjs](../../examples/electron/recent-documents/renderer.mjs)

## Operation and validation

Click Add recent document only when intentionally modifying your OS recent-document list. The selected path is the fixed local note. Tests neither add documents nor clear the user’s list.

## Reference and remaining work

[Electron’s Recent documents guide](https://www.electronjs.org/docs/latest/tutorial/recent-documents) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
