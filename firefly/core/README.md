# Desktop JavaScript API

`api.mjs` is the maintained ESM factory for the main-process desktop API. Native
services and the shared Mite builtin instances are injected explicitly.
`ipc-main.mjs` owns request handlers, pending replies and sender snapshots; the API
factory connects its cancellation to window/document lifecycle.
`message-ports/index.mjs` owns main-realm endpoint identity, start-gated queues,
transactional ownership moves and bounded weak-record/GC cleanup. It uses the
injected native binary codec and real event loop; it does not fabricate renderer
or utility-process endpoints. Native process/document handoff remains a separate
required binding. See the [port contract](../../docs/api/message-channel-main.md).
Renderer publication and world ownership live in `../renderer-bridge`; window integration
and the ESM DOM app-region installer live in `../native/qt`.

Native startup needs a synchronous factory before application modules run. The
committed `generated/api.js` is the embedding representation of this ESM source;
`generated/app-regions.js` invokes its DOM installer in the document. Edit the ESM
sources, then reproduce both adapters with the pinned build tools:

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/runtime-js
node tooling/runtime-js/build-firefly.mjs
node tooling/runtime-js/build-firefly.mjs --check
python3 tooling/check_firefly_js.py
```

Native builds verify the source/output hashes using Python without requiring Node.
The independent Mite generator and manifest do not depend on Firefly. Generated
files must stay in sync with their authored inputs; do not edit them directly.

The factory exposes only the implemented desktop subset. Unsupported methods fail
explicitly. Consult the [API reference](../../docs/api/index.md) and native test
gates for the current supported behavior; formatting and generation add no APIs.
