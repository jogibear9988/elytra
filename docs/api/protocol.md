# protocol

Status: partial main-process API with local macOS Release acceptance. Standard/
secure origins and default-session file callbacks load real custom-origin windows
and their resources. General Response handlers, partitions and broader Electron
compatibility remain unfinished.

## Register and load an application bundle

```js
import {app, BrowserWindow, protocol, session} from 'firefly';
import path from 'node:path';

protocol.registerSchemesAsPrivileged([
  {scheme: 'app', privileges: {standard: true, secure: true, supportFetchAPI: true}}
]);

await app.whenReady();
const files = new Map([
  ['app://bundle/index.html', path.join(import.meta.dirname, 'index.html')],
  ['app://bundle/renderer.mjs', path.join(import.meta.dirname, 'renderer.mjs')]
]);
session.defaultSession.protocol.registerFileProtocol('app', (request, callback) => {
  const file = files.get(request.url);
  callback(file ? {path: file, headers: {'Cache-Control': 'no-store'}} : {error: -10});
});
const window = new BrowserWindow({width: 1000, height: 700});
await window.loadURL('app://bundle/index.html');
app.on('window-all-closed', () => app.quit());
```

Use an explicit URL allowlist or implement containment and symlink authorization
before returning a path. No automatic URL-to-path conversion occurs. Files stay in
the native main process and stream through bounded byte credit; main JavaScript
receives metadata, not copied response bodies. Preload/page code cannot register
handlers or see native paths. The --electron-compat alias and CommonJS share the
same main singleton. Standalone Mite has no Firefly/LibWeb dependency.

## protocol.registerSchemesAsPrivileged(customSchemes)

Returns undefined. Call once before app readiness and before any WebContent helper
starts. Accepts at most 32 objects with scheme and optional privileges. Names use
canonical lowercase ASCII RFC spelling, start with a letter and contain at most 64
characters. Built-in/internal names, duplicates, unknown options and nonboolean
privileges are errors. Omitted privileges default to false.

| Privilege | Current contract |
| --- | --- |
| standard | Native/Rust tuple origin for hierarchical URLs with a valid nonempty host; preserve custom scheme and explicit port |
| secure | Mark a standard custom tuple origin trustworthy; requires standard: true |
| supportFetchAPI | Permit file-route registration and same-origin Window fetch; requires standard: true |
| bypassCSP, allowServiceWorkers, corsEnabled, stream, codeCache | true fails with ERR_FIREFLY_UNSUPPORTED_API; false requests no capability |

The current file transport requires supportFetchAPI: true even for navigation-only
apps. Without it, origin registration works but registerFileProtocol explicitly
refuses the scheme. This avoids granting unrequested fetch capability. Fetch authority
is enforced when main registers a route and grants its channels; the immutable
native/helper origin snapshot itself still carries only scheme/standard/secure.
Secure or fetch-enabled nonstandard schemes are explicitly unsupported.

Validate the entire batch before native publication. Invalid input does not consume
registration or partially install earlier entries. One success, including an empty
array, permanently consumes it. Native code independently validates the bounded
schema and atomically publishes an owned process table. Mutating input cannot change
it. Even an unconfigured helper launch seals registration. Origin privileges have
no reset/unregister operation. No CSP bypass, CORS, service workers, media policy or
V8 cache follows from registration.

## protocol.registerFileProtocol(scheme, handler)

Available after readiness. Returns true on installation, false if already handled;
a duplicate cannot replace the existing callback. The scheme must have been
registered with standard and supportFetchAPI above. The same method is available
through [session.defaultSession.protocol](session.md).

The handler receives a frozen object containing the actual URL and method (GET or
HEAD) and a callback. Other Electron request fields, including headers/referrer/
uploadData/webContentsId, are not supplied. Do not infer trusted initiator identity
from missing fields. Only same-origin top-level Window resources are currently
eligible; workers, child frames, cross-origin requests, POST and ranges are refused.

The callback accepts:

| Response | Meaning |
| --- | --- |
| Absolute native path string | Serve that checked regular file |
| {path, headers?} | Serve a file with a string/string-array header object; preserve repeated values |
| {error: negativeInteger} | Refuse; currently a generic network failure rather than exact Chromium error-number mapping |

Path metadata is bounded to 32,768 UTF-8 bytes and rejects NUL. At most 64 header
pairs and 128 KiB serialized reply metadata are accepted; response-head encoding
has its separate 32-KiB limit. Native code repeats schema/header validation. MIME
and Content-Length come from the actual opened file; overriding Content-Length,
invalid fields and hop-by-hop headers fail. Set-Cookie is filtered from renderer
responses. Cookie persistence/ASAR/ranges/redirects/arbitrary status codes are not
implemented by this file service. Only 200 file responses and refusal are supported.

Handlers may call back later, and different requests may finish out of order. A
returned value never substitutes for the callback. Throws/rejected handler promises
refuse outstanding requests. An unanswered request expires after 30 seconds. Up to
eight requests per document and 64 per native process, including unanswered ones,
reserve ownership before application code runs. Duplicate/stale callbacks cannot
open files. The callback returns undefined; observe load/fetch completion for actual
file success. Invalid response metadata throws and refuses the native request.

File routes automatically receive trusted navigation and exact-document grants only
for owned default-session windows. Main authorizes every pathname. No Node/Firefly
globals enter the page. Initial navigation buffering retains its 16-MiB bound;
ordinary file responses retain the native service's 64-GiB file-size bound.

## protocol.unregisterProtocol(scheme)

After readiness, returns whether a handler existed. Remove it, revoke its channels
and cancel pending navigation/document requests. Late callbacks cannot revive it or
affect a replacement generation. Removal does not undo origin registration. After
re-registering, navigate/reload to grant a fresh document channel; existing documents
are not automatically rebound in this implementation.

## protocol.isProtocolHandled(scheme)

After readiness, synchronously returns whether this default-session file handler
exists. Built-in protocols are not reported as custom handlers.

## Errors and remaining API

Incorrect types use ERR_INVALID_ARG_TYPE, invalid names/options/replies use
ERR_INVALID_ARG_VALUE, and JavaScript/reply-text budget checks use ERR_OUT_OF_RANGE. Native path/header
metadata validation uses ERR_INVALID_ARG_VALUE. Handler methods
before readiness use ERR_FIREFLY_NOT_READY. Late origin registration uses
ERR_FIREFLY_PROTOCOL_TOO_LATE; repeated registration or helper-launch sealing uses
ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED. Requested unsupported capabilities use
ERR_FIREFLY_UNSUPPORTED_API. File-open failures and explicit denial reject loadURL
with ERR_FAILED; pending loads cancelled by window/app lifecycle use ERR_ABORTED.

handle/unhandle, net.fetch, protocol interception, custom partitions, broader
request fields/privileges and in-place document rebinding remain unavailable. The
pinned Code-OSS entry still needs additional privileges/APIs. Without the engine
overlay it rejects Chromium enable-features; the explicit LibJS engine overlay
advances to unsupported protocol privilege corsEnabled. No workbench launch follows from this subset.

## Validation

check-firefly-protocol-registration checks real LibJS/native origin publication and
JS callback policy without windows. check-firefly-protocol-host checks actual main
ESM/CommonJS singleton/readiness exports. check-firefly-public-protocol batches one
hidden native window with real delayed JavaScript handlers, HTML/ESM/CSS, every-byte
4-MiB binary verification, HEAD, denial, missing files, revoked navigation/document
callbacks and handler generations. It uses ordinary public APIs, not a test transport.

```sh
cmake --build build/renderer --target check-firefly-protocol-registration check-firefly-protocol-host check-firefly-public-protocol --parallel 3
```

Local macOS Release passes; fresh hosted Linux execution remains required. Windows
production-helper/desktop support is still unavailable. See
[ADR 0126](../architecture/0126-public-file-protocol-routes.md).
Baseline: [Electron protocol](https://www.electronjs.org/docs/latest/api/protocol).
Only the subset described here is implemented.
