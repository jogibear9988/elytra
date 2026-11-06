# In-app purchases

**Firefly:** Unsupported in Firefly; the launcher fails explicitly before unsupported operations. Electron feature execution is manual/unverified.

Requires a signed Mac App Store sandbox app and configured products. Product lookup is implemented in this reference example; payment/receipt verification must be supplied before enabling purchases. No purchase is run by tests.

## Run

Build the [example catalog](index.md#build-and-run), then run:

```sh
build/renderer/bin/firefly build/desktop-examples/in-app-purchases/main.mjs
```

For the explicit Electron comparison build:

```sh
build/electron-reference/node_modules/electron/dist/Electron.app/Contents/MacOS/Electron build/desktop-examples-reference/in-app-purchases/main.mjs
```

This is reference code for a missing feature, not a successful Firefly demo.
No automated feature success is reported. Read the platform/setup requirements
before using the reference app; smoke mode is not a substitute for manual validation.

## API connections

- `inAppPurchase.getProducts` (not a supported Firefly desktop export)
- `inAppPurchase.purchaseProduct` (not a supported Firefly desktop export)

## Source files

- [index.html](../../examples/electron/in-app-purchases/index.html)
- [main.mjs](../../examples/electron/in-app-purchases/main.mjs)
- [purchase.mjs](../../examples/electron/in-app-purchases/purchase.mjs)
- [renderer.mjs](../../examples/electron/in-app-purchases/renderer.mjs)

## Operation and validation

Configure a signed store-sandbox app/product first. The UI performs product lookup only. purchase.mjs contains an optional queueVerifiedPurchase flow requiring a trusted main-process receipt-verification callback; no transaction is finished as purchased until verification returns true. The example does not supply production receipt validation, grant entitlements or charge during tests.

## Reference and remaining work

[Electron’s In-app purchases guide](https://www.electronjs.org/docs/latest/tutorial/in-app-purchases) is the API-design baseline. This page documents our own code and measured limits. Implement and validate the listed native Firefly APIs before changing this topic to supported.
