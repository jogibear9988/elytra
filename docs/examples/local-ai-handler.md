# Local AI handler

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Experimental Electron utility-process integration, not Firefly support. Requires a real local backend module; no fake model is included or inference claimed.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/local-ai-handler/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/local-ai-handler/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- `utilityProcess.fork` (not a supported Firefly desktop export)
- `session.registerLocalAIHandler` (not a supported Firefly desktop export)
- `localAIHandler.setPromptAPIHandler` (not a supported Firefly desktop export)
- `LanguageModel` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/local-ai-handler/index.html)
- [main.mjs](../../examples/electron/local-ai-handler/main.mjs)
- [renderer.mjs](../../examples/electron/local-ai-handler/renderer.mjs)
- [utility.mjs](../../examples/electron/local-ai-handler/utility.mjs)

## Operation and validation

Set ELYTRA_LOCAL_AI_MODULE to an absolute path exporting a real model-backed LanguageModelUtility subclass. The utility validates the origin and registers that class with localAIHandler. Enable the Prompt API in BrowserWindow and connect the worker to the session. Click Ask only after configuring the backend; no weights, fabricated inference or Node-addon compatibility are bundled.

## Reference and remaining work

[Electron’s Local AI handler guide](https://www.electronjs.org/docs/latest/tutorial/local-ai-handler) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
