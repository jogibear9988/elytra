# Application lifecycle

**Firefly:** Partial native macOS ARM64 slice; shared startup/assertions pass.

Readiness, cancellable normal quit/retry and forced exit. OS activation/dock/shutdown behavior is not implemented.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/lifecycle/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/lifecycle/main.mjs
```

The automated path adds --smoke. It checks the stated slice and closes the app;
it does not certify untested native interactions. See the catalog's coverage table.

Pass --exit-code=7 to the main script to exercise forced exit. The native runner
checks status 7 and the ready/when-ready/quit:7 trace against both runtimes,
without before-quit or will-quit. Normal --smoke checks cancellation and retry.

## API connections

- [app.isReady](../api/app.md)
- [app.whenReady](../api/app.md)
- [app.quit](../api/app.md)
- [app.exit](../api/app.md)
- `before-quit` (not a supported Firefly desktop export)
- `will-quit` (not a supported Firefly desktop export)
- `quit` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/lifecycle/index.html)
- [main.mjs](../../examples/electron/lifecycle/main.mjs)

## Operation and validation

Normal smoke records ready, when-ready, load, one deferred before-quit, retry, close/closed, will-quit and quit:0. Use --exit-code=7 to demonstrate forced exit before any window is created. The OS process status is then nonzero by design, not a sample failure.

## Reference and remaining work

[Electron’s Application lifecycle guide](https://www.electronjs.org/docs/latest/api/app) is the API-design baseline. This page documents our own code and measured limits. Broader edge cases and non-macOS Firefly desktop validation remain open.
