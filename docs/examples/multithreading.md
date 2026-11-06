# Multithreading

**Firefly:** Partial native macOS ARM64 slice; shared startup/assertions pass.

Browser-only module workers; Node/Mite APIs in workers and nodeIntegrationInWorker remain unsupported. File-worker loading and worker execution require their own native test result.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/multithreading/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/multithreading/main.mjs
```

The automated path adds --smoke. It checks the stated slice and closes the app;
it does not certify untested native interactions. See the catalog's coverage table.

## API connections

- `Worker` (not a supported Firefly desktop export)
- `postMessage` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/multithreading/index.html)
- [main.mjs](../../examples/electron/multithreading/main.mjs)
- [renderer.mjs](../../examples/electron/multithreading/renderer.mjs)
- [worker.mjs](../../examples/electron/multithreading/worker.mjs)

## Operation and validation

A real browser module worker receives a bounded integer, calculates 1..100 and posts 5050 back; the page terminates it after checking the value. No Node/Mite module access, shared worker, native addon or CPU-performance claim is made.

## Reference and remaining work

[Electron’s Multithreading guide](https://www.electronjs.org/docs/latest/tutorial/multithreading) is the API-design baseline. This page documents our own code and measured limits. Broader edge cases and non-macOS Firefly desktop validation remain open.
