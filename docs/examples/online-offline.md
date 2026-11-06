# Online and offline events

**Firefly:** Partial native macOS ARM64 slice; shared startup/assertions pass.

Smoke validates initial browser state/listener installation only. Real OS network transition delivery is a separate manual gate; onLine is not proof of internet reachability.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/online-offline/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/online-offline/main.mjs
```

The automated path adds --smoke. It checks the stated slice and closes the app;
it does not certify untested native interactions. See the catalog's coverage table.

## API connections

- `navigator.onLine` (not a supported Firefly desktop export)
- `online/offline events` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/online-offline/index.html)
- [main.mjs](../../examples/electron/online-offline/main.mjs)
- [renderer.mjs](../../examples/electron/online-offline/renderer.mjs)

## Operation and validation

Startup checks only the boolean browser hint and event-listener installation. Change network connectivity manually to assess event delivery; navigator.onLine is not a reliable remote-server reachability probe.

## Reference and remaining work

[Electron’s Online and offline events guide](https://www.electronjs.org/docs/latest/tutorial/online-offline-events) is the API-design baseline. This page documents our own code and measured limits. Broader edge cases and non-macOS Firefly desktop validation remain open.
