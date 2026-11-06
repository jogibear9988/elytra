# Web embeds

**Firefly:** Partial native macOS ARM64 slice; shared startup/assertions pass.

Runnable browser iframe slice. Native WebContentsView/webview and session/navigation controls remain unsupported; no Chrome extension embedding claim.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/web-embeds/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/web-embeds/main.mjs
```

The automated path adds --smoke. It checks the stated slice and closes the app;
it does not certify untested native interactions. See the catalog's coverage table.

## API connections

- `iframe` (not a supported Firefly desktop export)
- `WebContentsView` (not a supported Firefly desktop export)

## Source files

- [embedded.html](../../examples/electron/web-embeds/embedded.html)
- [embedded.mjs](../../examples/electron/web-embeds/embedded.mjs)
- [index.html](../../examples/electron/web-embeds/index.html)
- [main.mjs](../../examples/electron/web-embeds/main.mjs)
- [native-view.mjs](../../examples/electron/web-embeds/native-view.mjs)
- [renderer.mjs](../../examples/electron/web-embeds/renderer.mjs)

## Operation and validation

The iframe exchanges a fixed readiness string using postMessage and exact source-window checks, with no privileged payload. Native WebContentsView is a separate reference entry in native-view.mjs and remains unsupported in Firefly. This does not enable Electron webview or extensions.

## Reference and remaining work

[Electron’s Web embeds guide](https://www.electronjs.org/docs/latest/tutorial/web-embeds) is the API-design baseline. This page documents our own code and measured limits. Broader edge cases and non-macOS Firefly desktop validation remain open.
