# protocol

Status: partial main-process API with local macOS Release acceptance. Standard/
secure origins and default-session file callbacks load real custom-origin windows
and their resources, including a bounded opt-in CORS file-fetch subset. Modern
Response handlers, default-session net.fetch dispatch and native file interception
have local component/public acceptance; partitions and broader Electron compatibility
remain unfinished.

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
| corsEnabled | Enable native CORS checks for simple cross-origin GET/HEAD to this standard scheme; limited source/credentials scope below |
| bypassCSP, allowServiceWorkers, stream, codeCache | true fails with ERR_FIREFLY_UNSUPPORTED_API; false requests no capability |

The current file transport requires supportFetchAPI: true even for navigation-only
apps. Without it, origin registration works but registerFileProtocol explicitly
refuses the scheme. This avoids granting unrequested fetch capability. Fetch authority
is enforced when main registers a route and grants its channels; the immutable
native/helper origin snapshot carries scheme/standard/secure and the optional CORS flag.
Secure, CORS-enabled or fetch-enabled nonstandard schemes are explicitly unsupported.

Validate the entire batch before native publication. Invalid input does not consume
registration or partially install earlier entries. One success, including an empty
array, permanently consumes it. Native code independently validates the bounded
schema and atomically publishes an owned process table. Mutating input cannot change
it. Even an unconfigured helper launch seals registration. Origin privileges have
no reset/unregister operation. No CSP bypass, service workers, media policy or
V8 cache follows from registration.

## protocol.registerFileProtocol(scheme, handler)

Available after readiness. Returns true on installation, false if already handled;
a duplicate cannot replace the existing callback. The scheme must have been
registered with standard and supportFetchAPI above. The same method is available
through [session.defaultSession.protocol](session.md).

The handler receives a frozen object containing the actual URL and method (GET or
HEAD) and a callback. For admitted cross-origin requests only, request.headers is a
frozen object containing lowercase origin, derived from the native document binding;
it is not copied from renderer-supplied request metadata. For same-origin requests,
headers remains absent. Other request headers/referrer/uploadData/webContentsId are
not supplied. Do not infer trusted initiator identity from missing fields. Top-level
Window resources and the CORS subset below are eligible; workers, child frames,
POST and ranges are refused.

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

## Cross-origin file fetch (corsEnabled)

Register the **target** scheme with standard, supportFetchAPI and corsEnabled set
to true before readiness. Both source and target need active default-session file
handlers. Sources must be owned top-level custom-origin Window documents. HTTP,
file and opaque sources do not acquire these document channels. A different host
or explicit port also counts as cross-origin, even under the same scheme.

The renderer uses ordinary fetch(targetURL) or fetch(targetURL, {method: 'HEAD',
credentials: 'omit'}). Only simple GET/HEAD with mode 'cors' and default/omit
credentials is supported. Explicit credentials: 'include', cross-origin 'no-cors',
'same-origin', preflight/custom headers and ranges fail. No preflight API, CORS
worker/frame transport or credentialed cross-origin cookies are supplied.

The target callback can authorize an origin through its response headers:

```js
const allowedOrigin = 'app://bundle';
protocol.registerFileProtocol('resources', (request, callback) => {
  if (request.url !== 'resources://bundle/config.json' ||
      request.headers?.origin !== allowedOrigin) {
    callback({error: -10});
    return;
  }
  callback({path: authorizedConfigPath, headers: {
    'Access-Control-Allow-Origin': allowedOrigin,
    'Access-Control-Expose-Headers': 'X-App-Version',
    'X-App-Version': '1'
  }});
});
```

Use this after registering resources with the privileges above and after readiness;
authorize authorizedConfigPath in main. An exact source origin or wildcard * may
allow an uncredentialed response. Missing/mismatched/duplicate Allow-Origin values
fail before any file-body credit is granted. A policy refusal cancels only that
request; other document resources continue. Actual LibWeb CORS checks and exposed-
header filtering run: response.type is 'cors', arbitrary response headers stay
hidden unless exposed, and Set-Cookie never becomes page-visible. Successful
bodies retain streaming/backpressure, HEAD and document lifecycle checks.

CSP remains enforced independently. At the current Ladybird pin, use explicit
scheme sources such as connect-src 'self' resources: for permitted custom-scheme
connections. Host-source matching of custom opaque URL hosts is refused by upstream;
do not treat resources://bundle as an accepted CSP host source. Main must still
allowlist target URLs and native paths. A target's CORS flag alone authorizes neither
an origin nor a file. BypassCSP and broader Electron privilege parity remain absent.

## protocol.handle(scheme, handler) and protocol.unhandle(scheme)

After readiness, handle returns undefined and registers a function returning a genuine
Response or Promise<Response>. Custom schemes require standard and supportFetchAPI
registration above. Builtin file is also supported; HTTP(S) interception and other
builtin schemes are unavailable. Legacy and modern handlers share a collision table:
handle throws ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED for an existing handler;
registerFileProtocol returns false. unhandle returns undefined and removes a modern
handler. isProtocolHandled synchronously reports either kind.

```js
protocol.handle('app', async (request) => {
  if (new URL(request.url).pathname === '/health') {
    return new Response('ready', {headers: {'Content-Type': 'text/plain'}});
  }
  return new Response(null, {status: 404});
});
```

The handler receives a genuine captured Request. Renderer URL/method and the optional
cross-origin Origin header come from the native document/navigation binding. Other
request headers, upload bodies, webContentsId and initiatorOrigin are not supplied;
do not infer trusted initiator identity from ordinary Request defaults. Renderer
GET/HEAD only, top-level owned documents/navigation, no ranges, workers or child frames.
Main net.fetch carries its own genuine Request fields and shares this registry.

Responses preserve status 200–599, MIME/HTTP headers, binary data, nullable bodies and
known or unknown lengths. Response.error(), rejected handlers and body errors become
network errors. Body reads require consumer credit, at most 64 KiB per native chunk;
no complete-body/base64 IPC conversion occurs. One source chunk may be at most 1 MiB;
larger chunks fail explicitly. At most 64 pending process requests, 64 headers,
32 KiB encoded head metadata, 64 GiB streamed body and 30 seconds per native request
are accepted. Native navigation remains limited to 16 MiB. Declared Content-Length
must match actual EOF; HEAD and 204/205/304 do not deliver a body. HTTP framing headers
are refused, upstream CORS/CSP and forbidden-header filtering still apply, and custom
status text is not carried into renderer responses.

Request.signal and body readers are canceled on native navigation/document/channel
retirement, timeout, unhandle, window close and application shutdown. Late async
handlers, body results and generations cannot revive a retired request. Removing a
custom target conservatively retires the entire document channel; reload to rebind.

### Intercepting file

```js
protocol.handle('file', () => Response.error()); // Deny native file loading.
```

Register before opening windows when establishing an application-wide file policy.
New helpers inherit the native generation before creating a page. Existing helpers
receive ordered authenticated control updates and refreshed bindings for the actual
committed document. Missing/stale bindings fail closed while interception is active.
Updates precede subsequent main-issued grants/navigation; they do not atomically
cancel requests already started in a renderer before that update is received.

Interception runs before native file access, including the main load-file stat check.
A handler can return its own HTML or binary response without an existing file. It
never grants the renderer a native path or classifies file as a privileged custom
origin. Programmatic file requests are admitted only from an actual owned file
Window with matching native settings/document identity; other sources remain refused.
The narrow Fetch integration preserves prior CSP/mixed-content checks. Removing the
handler restores ordinary upstream file behavior, including its existing restrictions;
it does not grant general file fetch access. Re-enabling interception refreshes
existing committed file documents.

## protocol.unregisterProtocol(scheme)

After readiness, returns whether a handler existed. Remove it, revoke its channels
and cancel pending navigation/document requests. Removing a target scheme also
retires every document channel that has requested that scheme, including its other
outstanding resources. Reload the source document after replacement; this conservative
whole-channel revocation differs from per-request replacement. Late callbacks cannot revive it or
affect a replacement generation. Removal does not undo origin registration. After
re-registering, navigate/reload to grant a fresh document channel; existing documents
are not automatically rebound in this implementation.

## protocol.isProtocolHandled(scheme)

After readiness, synchronously returns whether this default session has a registered
legacy or modern handler, including an explicitly intercepted file scheme. An ordinary
builtin protocol without an application handler is not reported as handled.

## Errors and remaining API

Incorrect types use ERR_INVALID_ARG_TYPE, invalid names/options/replies use
ERR_INVALID_ARG_VALUE, and JavaScript/reply-text budget checks use ERR_OUT_OF_RANGE. Native path/header
metadata validation uses ERR_INVALID_ARG_VALUE. Handler methods
before readiness use ERR_FIREFLY_NOT_READY. Late origin registration uses
ERR_FIREFLY_PROTOCOL_TOO_LATE; repeated registration or helper-launch sealing uses
ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED. Requested unsupported capabilities use
ERR_FIREFLY_UNSUPPORTED_API. File-open failures and explicit denial reject loadURL
with ERR_FAILED; pending loads cancelled by window/app lifecycle use ERR_ABORTED.
Main fetch Response.error uses ERR_FIREFLY_PROTOCOL_NETWORK; malformed response
metadata/lengths use ERR_FIREFLY_PROTOCOL_DATA, bounded controller budgets use
ERR_FIREFLY_PROTOCOL_LIMIT, and retired main requests use ERR_FIREFLY_PROTOCOL_CANCELED
unless an original AbortSignal reason is available. Renderer failures remain network
errors rather than exposing these private main-process error details.

Custom partitions, HTTP(S)/other builtin interception, broader request fields,
workers/frames and additional privileges remain unavailable. The unchanged pinned
Code-OSS ProtocolMainService now passes genuine file-denial and managed-remote
404/500/binary/MIME consumer tests. The actual local-desktop/NLS startup advances
past this service to the native @vscode/policy-watcher dependency. No workbench or
workspace open/edit/save acceptance follows from this subset. See
[ADR 0175](../architecture/0175-public-response-protocols.md).

## Validation

check-firefly-modern-protocol batches genuine public main/renderer status, binary
streaming, HEAD, network errors, cancellation/generation replacement, legacy routes,
file policy removal/reactivation, and closed-owner cleanup. It is included in the
public-protocol gate. The local run uses the exact production application objects
with a static Qt offscreen frontend, without visible desktop interaction. Additional
native policy and real Host/GC tests exercise strict admission and malformed input;
Node/Mite policy tests distinguish controlled peers from native acceptance.

check-firefly-protocol-registration checks real LibJS/native origin publication and
JS callback policy without windows. check-firefly-protocol-host checks actual main
ESM/CommonJS singleton/readiness exports. check-firefly-public-protocol batches one
hidden native window with real delayed JavaScript handlers, HTML/ESM/CSS, every-byte
4-MiB binary verification, HEAD, denial, missing files, revoked navigation/document
callbacks and handler generations. The same batched window checks real cross-origin
bodies/HEAD, native source origins, exposed/hidden headers, missing/mismatched/duplicate
CORS authorization, disabled schemes, credentials/mode/preflight/range refusals,
CSP denial, channel survival after a refusal and target revocation/reload. It uses
ordinary public APIs, not a test transport. check-firefly-protocol-fetch independently
checks the actual LibWeb CORS path, including zero body credit for rejected metadata.

```sh
cmake --build build/renderer --target check-firefly-protocol-registration check-firefly-protocol-host check-firefly-public-protocol --parallel 3
```

Local macOS Release passes; fresh hosted Linux execution remains required. Windows
production-helper/desktop support is still unavailable. See
[ADR 0126](../architecture/0126-public-file-protocol-routes.md) and
[ADR 0134](../architecture/0134-simple-custom-protocol-cors.md).
Baseline: [Electron protocol](https://www.electronjs.org/docs/latest/api/protocol).
Only the subset described here is implemented.
