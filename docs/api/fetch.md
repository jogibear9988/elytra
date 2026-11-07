# Fetch, Headers, Request and Response

Status: partial native-runtime implementation on LibJS. Tested locally on macOS
against Node 24.15.0 with independent HTTP/TLS peers. The native Linux/Windows
CLI Fetch suites pass in the 65-suite baseline at da5e3e8; see [platform evidence](../status.md). These globals run in standalone Mite and Firefly's main process;
Ladybird renderer globals remain separate. No Node, V8 or LibWeb is embedded in Mite.

The current transport uses HTTP/1. HTTP/2 and HTTP/3 transport integration is
[planned later work](../roadmap.md#planned-http2-and-http3), below current platform work.

```js
const controller = new AbortController();
const response = await fetch('https://example.com', { signal: controller.signal });
console.log(response.status, response.headers.get('content-type'));
for await (const chunk of response.body) {
  console.log('received', chunk.byteLength);
}
```

## fetch(input, init?)

`input` is an absolute URL string, a URL or a Request. `init` overrides the Request
fields below. The returned Promise resolves to Response after response headers,
before the whole body arrives. Consume/cancel its body to finish owning the peer.
HTTP statuses such as 404/418/500 resolve normally; inspect `ok` or `status`.
Invalid requests, transport failures and refused certificates reject.

Only HTTP and HTTPS are supported. HTTPS uses the existing verified native
[TLS client](tls.md); it has no plaintext fallback. URL credentials and blocked
Fetch ports reject. No relative base URL is supplied in this server-side runtime.
Fragments are omitted from transport and network Response.url.

Each request uses one native connection, without Agents/pooling. Default request
headers include Accept `*/*`, Accept-Language `*`, User-Agent `mite`, Sec-Fetch-Mode
and Accept-Encoding `gzip, deflate`, unless explicitly supplied. Native incremental
[gzip/deflate decoding](decompression.md) reverses content-coding stacks while
preserving original encoded headers. Unsupported algorithms reject explicitly;
compression encoders, Brotli/zstd and a complete node:zlib surface remain absent.
The underlying HTTP client still enforces its header/framing policies.
Caller Host headers are ignored on the wire, matching the Node reference: HTTP
authority and HTTPS SNI/verification derive from the URL.

| Init field | Current contract |
| --- | --- |
| `method` | Defaults GET; common methods normalize to uppercase; invalid tokens and CONNECT/TRACE/TRACK reject |
| `headers` | Headers, iterable name/value pairs or a record |
| `body` | String/scalar-convertible value, ArrayBuffer/view, URLSearchParams, Blob/File/FormData or unlocked unused ReadableStream of Uint8Array chunks; GET/HEAD cannot have a body |
| `duplex` | Must be `half`; required for a directly supplied stream body |
| `signal` | AbortSignal or null; live/pre-aborted cancellation preserves the reason |
| `redirect` | `follow` (default), `manual` or `error` |
| `mode` | `cors` (default) or `same-origin` metadata; browser CORS enforcement is not provided; `no-cors` guards explicitly unsupported |
| `credentials` | `same-origin` (default), `omit`, `include` metadata; no cookie jar or ambient credentials |
| `cache` | `default` or `no-store`; no cache/revalidation implementation; other modes reject explicitly |
| `keepalive` | Boolean metadata; stream bodies with keepalive reject; no browser-navigation lifetime or connection-pool guarantee |
| `referrer` / `referrerPolicy` | Only default `about:client` / empty policy; other policies reject explicitly |
| `integrity` | Empty only; verification modes reject explicitly |
| `dispatcher` / `agent` | Custom Node dispatcher/agent extensions reject explicitly |

Expect and Upgrade request headers are explicitly unsupported. Known-size bodies
get an exact byte Content-Length; a supplied length must match the normalized
decimal length. Streaming uploads use the HTTP adapter's incremental framing and
pressure handling. A Uint8Array chunk is required even though ordinary Node writable
streams also accept strings. A failed upload destroys the request.

## Redirects and cancellation

Follow handles 301/302/303/307/308 with a 20-redirect limit. POST changes to GET on
301/302; 303 changes methods other than GET/HEAD to GET. Rewrites remove body
headers. Preserved methods replay snapshotted string/byte/URLSearchParams and immutable Blob/multipart bodies;
an arbitrary stream cannot be replayed and rejects. Cross-origin redirects remove
Authorization, Proxy-Authorization and Cookie. Each discarded response/old request
is disposed. Manual returns the redirect response with visible Location, as in the
server-side Node reference; error mode rejects instead of following.

Aborting before a request starts sends no HTTP request. During connection/header
wait, upload or response consumption, cancellation rejects with the original
`signal.reason`, destroys native work and cancels the upload producer. Reading a
response body after cancellation rejects with that reason. `response.body.cancel`
or reader.cancel closes the native peer. Convenience consumption locks the body;
cancel through the reader you own or the request signal.

Private signal subscriptions remain effective when a public listener calls
stopImmediatePropagation; the Node 24.15 Fetch reference can stay pending in that
case. An early completed server response, such as 413, cancels an unfinished upload
in Mite even when its pull Promise is pending. The measured Node reference delays
that producer cancellation. Tests retain these differences explicitly. Arbitrary
user-defined cancellation callbacks may themselves remain pending.

## Headers

`new Headers(init?)` creates an independent header list. Names are case-insensitive
and normalize to lowercase. Names must be HTTP tokens; values are byte strings,
trim edge HTTP whitespace and reject NUL/embedded CR/LF. Unicode outside ByteString
range rejects rather than corrupting native wire bytes.

| Member | Behavior |
| --- | --- |
| `append(name, value)` | Add a value |
| `set(name, value)` | Replace all values for the name |
| `delete(name)` | Remove all entries for the name |
| `get(name)` | Combined value or null; Cookie uses semicolon separation |
| `has(name)` | Boolean presence |
| `getSetCookie()` | Fresh array of separate Set-Cookie values |
| `entries()` / iterator | Sorted lowercase name/value pairs; Set-Cookie retains separate entries |
| `keys()` / `values()` | Corresponding iterators |
| `forEach(callback, thisArg?)` | Callback receives value, name and this Headers |

Public constructed Request/Response headers are mutable. Network responses,
Response.error and Response.redirect have immutable headers, including their
clones. Mutation attempts throw TypeError. Iterators observe subsequent list
changes; exact WebIDL/prototype/error-text parity is not claimed.

## Bodies and consumption

Request and Response expose `body` (ReadableStream or null) and `bodyUsed`.
String/view/ArrayBuffer/URLSearchParams inputs are snapshotted. Blob/File and
FormData have a separate [immutable multipart/data contract](blob-formdata.md). Text encodes UTF-8
with lone-surrogate replacement; views respect byte offset/length. Default content
types are `text/plain;charset=UTF-8` for text and
`application/x-www-form-urlencoded;charset=UTF-8` for URLSearchParams. Bytes and
arbitrary stream inputs do not invent a content type.

Network responses are byte streams supporting default and BYOB readers. Node/Web
bridges and byte queues bound transport prefetch; the whole response is not buffered
before fetch resolves. A BYOB caller must retain the returned view because its
input buffer transfers. EOF settles an outstanding zero-byte BYOB read.

`text()`, `json()`, `arrayBuffer()` and `bytes()` consume the complete body once.
The byte results have independent storage. Text/JSON decode UTF-8 with replacement
and BOM handling; invalid JSON rejects SyntaxError. These convenience methods
collect bytes in memory; use streaming for large bodies. `blob()` and `formData()` consume supported binary/URL-encoded/multipart bodies;
see the [data reference](blob-formdata.md) for their parser and memory limits.

`bodyUsed` becomes true on consumption, direct reader reads or cancellation.
Acquiring a reader alone locks but does not disturb the body. Consuming or cloning
a locked/disturbed body rejects TypeError. Null bodies remain unused and may be
read repeatedly as empty content. Cloning uses tee; a slower branch can accumulate
unread data. This is not a guarantee of bounded clone memory.

The disturbance adapter depends on the pinned portable Web Stream implementation's
private field, behind an internal host hook. Application mutation of that provider's
private properties is unsupported. Complete shared/resizable/detached-buffer,
WebIDL and cross-realm branding parity remains unverified.

## Request

`new Request(input, init?)` validates URL/options without opening a connection.
Readonly getters include url, method, headers, mode, credentials, cache, redirect,
referrer, referrerPolicy, integrity, keepalive, destination, duplex and signal.
Destination is empty; duplex is half; navigation flags are false. The signal is a
distinct follower of the source signal and preserves its eventual reason.

Constructing from another Request without replacing its body transfers consumption
ownership: the original becomes used/locked. `request.clone()` instead tees the
body and creates independent headers/signal followers. The two branches can each
be consumed once. These objects are not transferable across Firefly IPC.

## Response

`new Response(body = null, init?)` accepts status (default 200, range 200..599),
statusText (default empty) and headers. Statuses 204/205/304 cannot have a body,
even a zero-byte non-null body. Readonly getters include status, statusText, ok,
headers, type, url and redirected. Public responses have type default, empty URL
and redirected false; network responses have type basic and their final URL.

`response.clone()` tees an unused unlocked body and copies headers/metadata.
`Response.json(data, init?)` serializes JSON with application/json unless overridden.
`Response.error()` creates a type error response with status zero and null body.
`Response.redirect(absoluteURL, status = 302)` creates an immutable Location response
for one of the supported redirect statuses; it performs no network request.

## Validation and remaining work

Local differential tests cover 12 object workflows and 13 network workflows,
including independent response barriers, one-MiB BYOB reads, 256-KiB chunked upload,
clone/ownership, all redirect modes, credentials, truncation, cancellation and
peer-observed early connection closure. Separate policies retain
public-event suppression and early-upload differences; seven unavailable options
and three URL guards reject explicitly. An independent TLS peer observes ClientHello
and zero HTTP data on refused self-signed trust. A manual macOS default-trust probe
to https://example.com returns 200; it is not a repeatable hosted-platform gate.

Still open: further compression formats/encoding, complete data/WebIDL/WPT coverage, integrity/referrer/cache policy,
complete cookie/browser security behavior, connection pools/proxies/custom trust
configuration, exhaustive abort/redirect/GC races, and full WHATWG/Web Platform
Tests coverage. Fresh hosted bridge/fetch acceptance and native desktop distribution
remain separate requirements. No complete Node/Fetch/browser compatibility is claimed.

Run the [modern ESM client](../../examples/mite-fetch/client.mjs) with
`mite examples/mite-fetch/client.mjs [https://example.com]` or Node.
References: [Fetch standard](https://fetch.spec.whatwg.org/),
[Node 24.15 globals](https://nodejs.org/download/release/v24.15.0/docs/api/globals.html#fetch).
