# Represented file

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

macOS native represented-document icon/edited marker. Firefly methods are absent; an HTML filename is not equivalent.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/represented-file/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/represented-file/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- [BrowserWindow.setRepresentedFilename](../api/browser-window.md)
- [BrowserWindow.setDocumentEdited](../api/browser-window.md)

## Source files

- [index.html](../../examples/electron/represented-file/index.html)
- [main.mjs](../../examples/electron/represented-file/main.mjs)
- [note.txt](../../examples/electron/represented-file/note.txt)
- [renderer.mjs](../../examples/electron/represented-file/renderer.mjs)

## Operation and validation

On macOS inspect the native represented-document icon and edited indicator; Command/Control-click the window title. Test the real local note path. Firefly must implement both methods before this can be accepted there.

## Reference and remaining work

[Electron’s Represented file guide](https://www.electronjs.org/docs/latest/tutorial/represented-file) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
