# Spellchecker

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Native spell suggestions/context menu require manual validation and dictionaries. Firefly spellcheck constructor option/session APIs are unsupported.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/spellchecker/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/spellchecker/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- [webContents.session](../api/web-contents.md)
- `Menu.buildFromTemplate` (not a supported Firefly desktop export)
- [webContents.replaceMisspelling](../api/web-contents.md)

## Source files

- [index.html](../../examples/electron/spellchecker/index.html)
- [main.mjs](../../examples/electron/spellchecker/main.mjs)
- [renderer.mjs](../../examples/electron/spellchecker/renderer.mjs)

## Operation and validation

Type a misspelling and right-click. Suggestions and Add word use the real session/spellchecker methods. Language selection is skipped on macOS, where native language detection applies; dictionary availability/download policies need separate validation.

## Reference and remaining work

[Electron’s Spellchecker guide](https://www.electronjs.org/docs/latest/tutorial/spellchecker) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
