# Native file drag and drop

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Manual native drag interaction. The drag file is fixed in main; renderer input is not a filesystem path. Firefly nativeImage/startDrag/IPC are absent.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/native-file-drag-drop/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/native-file-drag-drop/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- [webContents.startDrag](../api/web-contents.md)
- `ipcMain.handle` (not a supported Firefly desktop export)
- `nativeImage` (not a supported Firefly desktop export)

## Source files

- [icon.png](../../examples/electron/native-file-drag-drop/icon.png)
- [index.html](../../examples/electron/native-file-drag-drop/index.html)
- [main.mjs](../../examples/electron/native-file-drag-drop/main.mjs)
- [note.txt](../../examples/electron/native-file-drag-drop/note.txt)
- [renderer.mjs](../../examples/electron/native-file-drag-drop/renderer.mjs)

## Operation and validation

Drag the note into Finder/file manager and verify the native copied file. Main supplies fixed local note/icon paths; the renderer cannot choose an arbitrary filesystem source. Validate cancellation and repeated drag manually; an ordinary HTML drag is not native startDrag acceptance.

## Reference and remaining work

[Electron’s Native file drag and drop guide](https://www.electronjs.org/docs/latest/tutorial/native-file-drag-drop) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
