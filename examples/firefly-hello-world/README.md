# Firefly hello-world

The main entry, preload source and page script use modern ES modules. Build and
run the example from the repository root after building Firefly:

```sh
python3 tooling/build_hello.py
build/renderer/bin/firefly build/firefly-hello-world/main.mjs
```

Node/npm and the pinned esbuild package are build tools. The output keeps main.mjs
as ESM and bundles preload.mjs into the CommonJS payload consumed by the isolated
sandbox. This follows the [Electron sandboxed-preload bundling model](https://www.electronjs.org/docs/latest/tutorial/esm#sandboxed-preload-scripts-cant-use-esm-imports).
The app runs on Mite/LibJS and Ladybird; it does not ship Node or a bundler runtime.
CommonJS remains supported for existing applications and compatibility tests.

The preload publishes a small frozen desktop API; the page calls its add function
and has its own click counter. The main entry uses whenReady().then(createWindow)
so module initialization does not wait for the application readiness event.
Closing the last window quits. For the explicit native smoke path:

```sh
build/renderer/bin/firefly build/firefly-hello-world/main.mjs --smoke
```

This verifies that the module page and bundled preload initialize, then closes the
window. The release driver tests the sample in both Firefly and the pinned Electron
reference and launches the packaged Firefly.app through its native launcher.
