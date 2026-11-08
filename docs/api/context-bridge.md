# Preload and contextBridge

Availability: partial native macOS ARM64 Firefly implementation. This API publishes
an isolated renderer API to a document. [IPC](ipc.md) supports the bounded
invoke/handle request/reply subset in configured top-level preloads.

## Author an ESM preload

Application examples use ESM sources. The current sandbox executes a bundled
CommonJS payload, following the same bundling pattern described in
[Electron's sandboxed-preload ESM guidance](https://www.electronjs.org/docs/latest/tutorial/esm#sandboxed-preload-scripts-cant-use-esm-imports).
Build it from modern preload source before launching:

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/runtime-js
node tooling/bundle_preload.mjs path/to/preload.mjs path/to/preload.cjs
```

```js
// main.mjs
import {app, BrowserWindow} from 'firefly';
import {fileURLToPath} from 'node:url';
app.on('window-all-closed', () => app.quit());
app.whenReady().then(async () => {
  const window = new BrowserWindow({
    webPreferences: {preload: fileURLToPath(new URL('./preload.cjs', import.meta.url))}
  });
  await window.loadFile('index.html');
}).catch(error => {console.error(error); app.exit(1);});
```

```js
// preload.mjs (authored source)
import {contextBridge} from 'firefly';
contextBridge.exposeInMainWorld('desktop', {
  label: 'My app',
  add: (left, right) => left + right,
  answer: async () => ({value: 42})
});
```

The [hello-world source](../../examples/firefly-hello-world/main.mjs) and
`python3 tooling/build_hello.py` demonstrate that build/launch path. CommonJS is
still supported for existing apps; no CommonJS source is required in the sample.

```js
// ordinary document script
console.log(desktop.label, desktop.add(20, 22));
console.log((await desktop.answer()).value);
```

The preload runs in its own LibJS realm before the first author script in each
loaded top-level file/HTTP/HTTPS document. Local files and file-to-HTTP process
replacement are tested; HTTPS-specific preload behavior has not been validated.
No preload is installed in the initial about:blank or child frames, including
srcdoc. An application must control which documents it loads: an exposed API is
available to every supported top-level document loaded into that window. A
same-origin child can access its parent's published API using normal DOM access.
There is no origin allowlist or public navigation-policy API yet.

The main process reads a UTF-8 source snapshot when constructing the BrowserWindow.
Navigation runs that snapshot in a fresh document world; editing the source file
does not change an existing window's preload. Separate windows select independent
sources. No source is fetched by the document or installed as a document script.

## Sandboxed preload environment

Only `require('firefly')`, exporting contextBridge and the bounded ipcRenderer subset, is implemented. Other modules,
including node:fs, relative modules and electron, throw ERR_FIREFLY_UNSUPPORTED_API.
There is no Mite/Node process, Buffer or filesystem loader. The CommonJS wrapper
provides module, exports, __filename and __dirname; this is not a full Node module
implementation. Direct ES module preload paths (.mjs) explicitly reject; bundle authored ESM first. Bundle dependencies
into a single CommonJS preload when they do not require unsupported capabilities.
Browser APIs depend on the implemented isolated-world boundary; timer, observer
and DOM callback parity remains incomplete.

## contextBridge.exposeInMainWorld(apiKey, api)

Returns undefined. Publish a nonempty string key (up to 256 UTF-16 code units)
and a supported data/function graph. Copied objects/arrays are frozen, graph
cycles/aliases are retained and functions become guarded proxies. The document
property is enumerable, nonwritable and nonconfigurable. Existing names reject;
failed publication does not leave a partially installed API. The source object
itself is not frozen. This is a copied snapshot, not live shared object state.

Supported data includes undefined, null, booleans, numbers, strings and plain
objects/arrays with own data properties. Functions require validated source-world
ownership. Accessors, proxies, bound functions, custom prototypes, symbols, DOM/native
objects and promises embedded directly in an API graph are unsupported. Function
arguments/results pass through bounded data copies; document-supplied function arguments
are proxied as described below, and a returned Promise is bridged.
Page getters/proxies are rejected without running their traps. This stricter policy
and bounded graphs are intentional differences from general Electron value support.

Per-world publication accepts at most 64 names and 64 function endpoints, with
additional snapshot depth/node/byte and pending-work budgets enforced by the native
bridge. Unsupported graphs and exhausted budgets throw; no unlimited graph or
universal browser-object membrane is provided. Foreign exceptions are sanitized;
Node/Electron error text and stacks are not preserved. Navigation/detach revokes
function endpoints and pending work; copied inert data can remain reachable.

Only expose narrow functions appropriate for all documents in the window. A
function's ability to access its isolated closure is not privileged access to the
main process. Use a fixed [ipcRenderer.invoke](ipc.md) action for that boundary;
main-process imports expose app/BrowserWindow/ipcMain. executeInMainWorld and
additional contextBridge methods are not implemented.

## Document callbacks

A document can pass its ordinary functions to an exported API, including functions
nested in plain data graphs. The preload receives frozen, revocable proxies.
Calls copy/freeze data into the document realm; synchronous data and native Promise
results copy back. The callback receives undefined as its receiver (ordinary sloppy
JavaScript receiver conversion still applies). Errors become destination-owned
TypeErrors; foreign exception contents, getters and privileged source stack frames
are not exposed. Promise observation bypasses user then/constructor/species hooks.

Keep IPC events inside the preload and pass only the application payload:

```js
// preload.mjs
import {contextBridge, ipcRenderer} from 'firefly';
const listeners = new Set();
ipcRenderer.on('value', (_event, value) => {
  for (const callback of listeners) callback(value);
});
contextBridge.exposeInMainWorld('values', {
  on(callback) { listeners.add(callback); },
  off(callback) { listeners.delete(callback); }
});
```

```js
// document
const listener = value => console.log(value);
values.on(listener);
values.off(listener);
```

Firefly preserves proxy identity for the same live document function across exported
methods, enabling this on/off pattern. The cache is weak: unreferenced proxies are
collectable. This identity guarantee and frozen callback input are Firefly behavior;
the tested Electron reference requires application-managed subscription identities
or per-subscription disposal instead of relying on identity across methods.

There are at most 64 live callback endpoints and 64 pending callback Promises per
document registry, 64 arguments per call, and 32 nested callback/API entries.
Invalid argument capture rolls back newly created proxies. Delivery commits before
application code runs, so its effects survive a later application exception.
Navigation/detach revokes proxies and rejects pending bridges; unrelated principal
Promise continuations keep running. Late results cannot revive a revoked endpoint.

Callback arguments/results themselves must be supported data: higher-order function
results, callable proxies/bound functions, binary values, ports and native objects
remain unsupported. The native ipcRenderer module is a specific exception: crossing
from preload to document replaces that exact module with a frozen empty ordinary
object, including event.sender, nested API values and synchronous/Promise results.
No methods, expandos, getters or listener bookkeeping are copied from the module.
An individual raw IPC method still rejects, as does a copied object containing one.
Normal IPC serialization has no module-redaction exception. Prefer the narrow
payload wrapper above even though the supported empty-port IPC event now crosses
this boundary. This does not yet support Code-OSS's unchanged full preload graph
(source accessors, process/options, webFrame/webUtils and ports remain outstanding).

This follows [Electron's documented ipcRenderer boundary](https://www.electronjs.org/docs/latest/api/context-bridge#exposing-ipcrenderer).
The actual Electron 45.2.3 positive fixture strips the IPC methods but retains own
EventEmitter bookkeeping; Firefly deliberately drops that data too and freezes the
empty destination object. Reference tests compare absence of IPC methods, with
exact emptiness/freezing checked separately for Firefly. See
[ADR 0138](../architecture/0138-document-callback-bridge.md) and
[ADR 0139](../architecture/0139-private-ipc-module-redaction.md).

## Files, budgets and errors

| Condition | Current behavior |
| --- | --- |
| Non-string preload | ERR_INVALID_ARG_TYPE |
| Relative path, NUL or filename over 4096 UTF-8 bytes | ERR_INVALID_ARG_VALUE |
| Missing/nonregular/unreadable file or invalid UTF-8 source | ERR_FIREFLY_PRELOAD_READ_FAILED |
| Source over 64 KiB, over 16 configured windows or over 1 MiB total source | ERR_FIREFLY_PRELOAD_TOO_LARGE |
| .mjs preload / unsupported require | ERR_FIREFLY_UNSUPPORTED_API |
| Syntax error or uncaught preload exception | Renderer exits; pending load rejects with ERR_FAILED |

The current failure policy terminates the renderer instead of implementing
Electron's preload-error event/continue behavior. Other documents sharing that
renderer can be affected. At most 32 active preload worlds are retained per helper;
exhaustion also terminates it. Diagnostics omit application exception contents.

## Evidence and remaining work

[Public preload tests](../../tooling/tests/test_public_preload.py) run real Electron and
Firefly windows for first-script installation, frozen nested APIs, asynchronous
results, three windows, no child-frame installation, navigation and file-to-HTTP
process replacement. Firefly-specific tests cover invalid paths/files, exact 64 KiB
acceptance, restricted CommonJS metadata, getter rejection and fail-closed syntax/
throw behavior. Native SDK/helper gates separately exercise reentry, graph rollback
and detached callback/Promise revocation. Packaged tests use the production helper.
The [native callback gate](../../firefly/tests/isolation/CallbackBridgeTest.cpp)
checks identity, weak GC, capture rollback, bounded reentry, exact callback/Promise
limits, clean stacks, sanitized failures and detach. The
[public IPC batch](../../tooling/tests/test_ipc_outbound.py) also verifies real
document subscriptions, Promise results and API reentry through the production
helper, with a separate Electron common-contract reference. These scoped tests
are not a complete security review or platform certification.

See [security](../guides/security.md), [compatibility](../compatibility/firefly-0.1.md)
and [ADR 0068](../architecture/0068-public-commonjs-preload.md).
