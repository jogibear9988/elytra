# Taskbar customization

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Windows-only native taskbar UI. Firefly Windows desktop and these methods are not implemented; macOS/Linux checks are explicit platform errors.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/taskbar-customization/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/taskbar-customization/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- [BrowserWindow.setOverlayIcon](../api/browser-window.md)
- [BrowserWindow.setThumbarButtons](../api/browser-window.md)

## Source files

- [icon.png](../../examples/electron/taskbar-customization/icon.png)
- [index.html](../../examples/electron/taskbar-customization/index.html)
- [main.mjs](../../examples/electron/taskbar-customization/main.mjs)
- [renderer.mjs](../../examples/electron/taskbar-customization/renderer.mjs)

## Operation and validation

Run on Windows and inspect the taskbar overlay and thumbnail button. The button logs the document title. Confirm real taskbar behavior; native Firefly Windows windows/packaging and these APIs remain unfinished.

## Reference and remaining work

[Electron’s Taskbar customization guide](https://www.electronjs.org/docs/latest/tutorial/windows-taskbar) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
