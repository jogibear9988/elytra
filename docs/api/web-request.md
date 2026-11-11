# webRequest

Status: partial default-session request interception. This is the Session property,
not a top-level module. Unsupported event stages and decisions fail explicitly.
See the [Session](session.md), [desktop networking](desktop-net.md) and
[frame](web-frame-main.md) references.

```js
import { app, session } from 'firefly';

await app.whenReady();
const requests = session.defaultSession.webRequest;
requests.onBeforeRequest({ urls: ['https://*.example.com/*'] }, (details, callback) => {
  callback({ cancel: details.url.endsWith('/blocked') });
});

requests.onHeadersReceived((details, callback) => {
  callback({ responseHeaders: { ...details.responseHeaders, 'X-Application': ['Firefly'] } });
});
```

## Registration

`onBeforeRequest([filter,] listener)`, `onBeforeSendHeaders([filter,] listener)`
and `onHeadersReceived([filter,] listener)`
each retain one listener. A later registration replaces that stage's listener;
passing null removes it. They do not accumulate like EventEmitter listeners.
Registration requires app readiness and native enforcement.

An omitted filter matches all requests. A supplied filter requires `urls`, an array
of patterns. `excludeUrls` and `types` are optional. Includes are combined with OR;
an exclusion overrides an include. A nonempty types array further restricts matches.
Empty URL/type arrays currently match all, following the tested Electron version.
Prefer `['<all_urls>']` instead of an empty URL array.

Patterns support explicit schemes, `*://` for HTTP/HTTPS, `*` hosts, `*.example.com`
host suffixes, exact IPv6 literals, numeric or wildcard ports, and `*` in the path
and query. Resource filters accept mainFrame, subFrame, stylesheet, script, image,
font, object, xhr, ping, cspReport, media and webSocket. Resource details can also
report other. Filter arrays are limited to 256 URL patterns, each at most 8192
characters. Unknown filter options and unsupported decisions fail explicitly.

## Details and decisions

All three listeners receive `(details, callback)`. Details contain a stable numeric id,
url, method, resourceType, referrer, timestamp and frame. Renderer requests also
carry webContentsId, webContents and a canonical native frame proxy. Frame data is
derived from the actual helper, page and document, independently of preload IPC.
Main-process net requests have frame null and no webContents properties.
An available initiatorOrigin describes the native requesting document.

Call `callback({ cancel: true })` to reject the request. `{ cancel: false }` or `{}`
allows it. Duplicate or retired callbacks have no effect. A thrown listener or an
invalid decision cancels unresolved work. Before-request cancellation occurs before
the HTTP transport or privileged custom/file handler is invoked.

The send-headers listener receives requestHeaders as header-name/string pairs.
Return `{ requestHeaders: { ...details.requestHeaders, 'X-Application': 'Firefly' } }`
to replace the request header list. Omitted fields in that replacement are removed.
Before-request runs first; cancellation skips send-headers. Both use the existing
authenticated held request before transport, so custom header additions/removals
alter the actual native Fetch request before CORS preflight and main net transports
before every redirect hop.

This is a partial snapshot of explicitly captured request headers. SDK transport
headers such as automatic Cookie, Host and framing fields are added later; final
wire-header visibility and casing are not promised. Registration does not enable
a separate final transport-header event.

Headers listeners additionally receive statusCode and responseHeaders, a record of
header-name to string arrays. Native Fetch currently normalizes names to lowercase;
exact wire casing and statusLine are unavailable. No HTTP version is fabricated.
Return `{ responseHeaders: record }` to replace the entire head; copy existing
headers when adding fields. String values are accepted and converted to arrays.
HTTP and owned custom/file headers are intercepted before CORS and response
exposure. Body streaming and cancellation stay with the original transport.

Header names must be valid HTTP tokens; values must be byte strings without
CR/LF or forbidden control characters. Limits are 128 names, 128 values per name
and 32 KiB of header data, with an additional total-field limit in the renderer.
Changing or removing content-length, transfer-encoding, connection, trailer,
upgrade, keep-alive, proxy-connection or te is unsupported. Unchanged copies are
accepted. Request-header decisions also cannot add, remove or change Host.
RedirectURL, statusLine overrides and the other webRequest event stages remain
unsupported.

## Scope and lifecycle

The native control channel authenticates each helper with a random cookie and its
actual process identity. Native document ownership is checked on admission and
completion. Replacement, abort, navigation, owner loss, channel loss, timeout and
shutdown cannot authorize a retired continuation. Pending decisions are bounded
to 64 in the main policy and native broker, 16 per helper, with 30-second deadlines.
Native request IDs are odd; main net IDs are even and neither is reused.

The current renderer scope covers document requests and authenticated top-level
navigation. Worker requests and unbound/subframe navigation fail closed when
interception is active. WebSocket handshakes currently bypass upstream Fetch and
therefore fail explicitly before connect while a blocking handler is active. Worker
policy reads are thread-safe; actual worker/socket interception remains open.
This is not full browser-session coverage. Existing helper
policy changes propagate over an ordered asynchronous channel; registration is not
an atomic barrier across all already-running helpers. New helpers receive the
policy before VM initialization. Filter allowance never grants filesystem access
or bypasses the independent protocol ownership rules.

Main net.fetch and net.request consult the same policy before every redirect hop
and before exposing response headers. Utility processes currently accept no
`session` or `partition`: their Electron-net transport uses an independent system
context and does not inherit `defaultSession.webRequest` handlers. Registering
main filters and running these no-session utilities can coexist. Explicit utility
session/partition selection and main-process authentication forwarding remain
unsupported. Direct node:http, node:https and global fetch belong to Mite,
outside the desktop Session API.

This distinction follows [Electron45.2.3's utility network factory](https://github.com/electron/electron/blob/v45.2.3/shell/browser/api/electron_api_utility_process.cc#L539-L557):
only the explicit-session branch installs its ProxyingURLLoaderFactory for
webRequest interception; the no-session branch selects the system network context.
See also the [versioned utility options](https://github.com/electron/electron/blob/v45.2.3/docs/api/utility-process.md).

The public API follows the [Electron WebRequest reference](https://www.electronjs.org/docs/latest/api/web-request)
and [filter structure](https://www.electronjs.org/docs/latest/api/structures/web-request-filter)
within these stated limits. Full workbench, Windows/Linux desktop and packaged
release acceptance require separate tests.
