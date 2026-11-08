# net — desktop HTTP and network availability

Status: **partial, locally tested on macOS** in the main and utility processes.
Firefly uses LibJS and Mite's native HTTP/TLS transport. It does not embed Chromium,
Node or V8. Native availability/proxy discovery is currently unavailable on Linux
and Windows, so these desktop requests fail explicitly there. Mite's independent
[node:net](net.md), [HTTP](http.md) and [HTTPS](https.md) APIs have separate contracts.

```js
import { app, net } from 'firefly';

await app.whenReady();
const response = await net.fetch('https://example.com/');
console.log(response.status, await response.text());
```

With --electron-compat, the same export is available from electron. Owned utility
children also expose net; their module alias follows the parent's compatibility
mode. Utility networking starts before Qt or a desktop application is initialized.
The independent Mite fork-child role gains no Firefly networking capability.

## Network state

net.isOnline() and the read-only net.online property return the latest passive
native network-availability observation. macOS uses Network.framework NWPathMonitor:
satisfied/satisfiable paths mean available, unsatisfied means unavailable. This is
not proof that any Internet destination is reachable. No probe request, permission
prompt or network-setting change is performed.

Before the first native update, queries throw ERR_FIREFLY_NETWORK_STATE_PENDING.
Invalid observations fail explicitly rather than returning offline. These direct
snapshots do not require app.ready; net.request/net.fetch do. Calling after service
retirement throws ERR_FIREFLY_APP_EXITED. Unsupported platforms throw
ERR_FIREFLY_NETWORK_UNAVAILABLE.

## net.fetch(input[, init])

Returns a Promise of a genuine Response with a readable body. String/Request inputs,
HTTP(S) URLs, methods, headers, supported bodies, AbortSignal, manual/follow/error
redirect modes and gzip/deflate decoding use captured original Mite primitives.
Public global fetch and HTTP module replacements cannot replace that private
transport. Response URL/redirected metadata, header guards and clones are preserved.

Both admission and each redirect check the current system-proxy snapshot. A
configured proxy, SOCKS, PAC/WPAD or scoped/supplemental route is explicitly
unsupported; malformed/undetermined proxy snapshots fail closed. The current
conservative detector rejects a configured route even if an exception might bypass
it. Proxy execution, exceptions, tunneling and authentication remain required work.

At most 20 redirects are followed. Origin changes, including a port-only change,
strip Authorization, Proxy-Authorization and Cookie. The Electron reference
forwarded an explicit Authorization header in the tested redirects; Firefly
intentionally retains stricter stripping. Method/body rewriting follows the tested
301/302 POST and 303 behavior. Redirects that would replay a nonempty fetch body
are explicitly unsupported; buffered ClientRequest bodies have a separate replay path.

Supported explicit credentials mode is omit; session credentials are unavailable.
Cache default/no-store are accepted, but there is no shared HTTP cache. Explicit
bypassCustomProtocolHandlers:true is accepted for the built-in HTTP(S) path.
Custom/file/data/blob schemes, custom interception, webRequest hooks, session/
partition selection, session cookies/authentication, priority, nonempty referrer/
integrity policies, disabled TLS verification and custom agents fail explicitly.
This is an isolated stateless transport: it does **not** share renderer/default-
session cookies, authentication cache or proxy behavior with Electron's session stack.

The body remains owned after headers resolve. EOF or cancel releases its request
slot; service shutdown aborts outstanding exchanges and bodies. Consume or cancel
unused bodies. There are 64 concurrent transport slots. No response callback or
completed local write proves that a remote application consumed the data.

## net.request(options[, responseCallback])

Returns a Writable ClientRequest. A string specifies the URL. Object options support
url (or href), or protocol/host/hostname/port/path, plus method, headers and redirect.
The optional response callback is the same as a response listener and supports the
actual Code-OSS raw-request hook. agent:null/undefined means unspecified; non-null
custom agents are unsupported. rejectUnauthorized:true preserves verification;
false is rejected. The stateless/session/proxy limits above also apply.

```js
const request = net.request({ url: 'https://example.com/', method: 'GET' });
request.on('error', console.error);
request.on('response', (response) => {
  console.log(response.statusCode, response.headers);
  response.on('data', (chunk) => console.log(chunk.toString()));
});
request.end();
```

| Member | Implemented behavior |
| --- | --- |
| setHeader/getHeader/removeHeader | Validate names/values, preserve caller values and snapshot arrays. Headers lock when writing begins or end is called. |
| write/end | Writable callbacks/pressure and buffered upload. Copies accepted chunks; requests start at end, matching Electron's default buffered path. Body limit 16 MiB. |
| abort | Retires the exchange/response and emits abort. Repeated abort is harmless. |
| followRedirect | Only during a manual redirect listener; otherwise throws ERR_INVALID_STATE. |
| chunkedEncoding | False selects the buffered path. True is explicitly unsupported; assignment must precede writing and can happen only once. |
| getUploadProgress | Explicitly unsupported; no fabricated progress counters. |

ClientRequest has 64 retained slots, including requests waiting for end. Responses
provide actual statusCode/statusMessage/headers and Readable body semantics, including
pause/resume and cancellation. Header values follow the tested Electron array shape;
Set-Cookie values remain separate. Bodies are automatically decoded. rawHeaders,
HTTP-version fields and trailers throw unsupported errors because Fetch does not
supply authoritative raw metadata. Neither pause tests nor Writable pressure claim
a hard limit on kernel buffers or all upstream buffering.

response/error/abort and redirect events are tested. Manual redirect supplies
(statusCode, method, redirectURL, headers); followRedirect must be called synchronously
during that event. Without it, the request is cancelled. Error redirects reject;
follow redirects apply the same origin credential stripping and per-hop proxy checks.
Closing the application retires every owned request even if public instance destroy
methods have been replaced. This does not implement Electron's complete login,
proxy authentication, socket or progress event surface.

## Validation

Owned independent loopback peers exercise HTTP bytes/uploads, gzip, credential
redirects, request callbacks, response pause/resume, cancellation before and after
headers, and main/utility isolation. A real TLS handshake rejects an untrusted
certificate before sending HTTP. A separate optional public HTTPS probe verifies
positive delivery using normal certificate verification; it is not a default CI
Internet dependency. No keychains, trust stores or network settings are changed. The unchanged pinned
Code-OSS wake monitor (32 compiled dependencies) and nodeRequest consumer (61)
pass with native availability and the real request hook. This does not establish
full request-service/session/proxy or workbench acceptance.

```sh
python3 tooling/tests/test_firefly_net_policy.py --mite build/release-mite/bin/mite
cmake --build build/renderer --target check-firefly-network-state-provider check-firefly-net-native check-firefly-net-transport
build/renderer/bin/firefly-net-native-probe firefly/tests/net/public-https.mjs
```

See [current Code-OSS acceptance](../compatibility/vscode.md),
[Electron net](https://www.electronjs.org/docs/latest/api/net) and
[ClientRequest](https://www.electronjs.org/docs/latest/api/client-request).
