# node:https

Status: partial native HTTPS/1 client, locally verified on macOS ARM64. Uses the
same ClientRequest/IncomingMessage implementation as node:http over verified native
TLS; there is no separate JavaScript engine or plaintext fallback. Changed-source
Windows/Linux acceptance remains pending. HTTPS servers and Agents are unsupported.

```js
import { get } from 'node:https';
import { readFileSync } from 'node:fs';
const request = get('https://localhost:8443/data', {
  ca: readFileSync('ca.pem'),
  signal: AbortSignal.timeout(5000),
  agent: false
}, (response) => {
  response.setEncoding('utf8');
  response.on('data', (text) => console.log(text));
  response.on('error', (error) => console.error(error.code));
});
request.on('error', (error) => console.error(error.code, error.cause));
```

## request and get

`request(options[, callback])` and `request(url[, options][, callback])` return the
same http.ClientRequest class as node:http. `get` calls request then end. The
callback subscribes once to response, whose value is the same http.IncomingMessage
class. Its socket is a TLSSocket. ESM/CommonJS and default/named exports share the
same builtin instance.

Default protocol is https: and port 443. Non-HTTPS URLs/protocols reject with
ERR_INVALID_PROTOCOL; node:http continues rejecting https:. Host header generation
omits the default port. URL pathname/query, URL/options authentication, method,
headers, streaming writes, timeout, response framing and error behavior use the
supported [HTTP contract](http.md). A connection is owned by one request; agent:false
and omission select that policy. Agents, pooling and globalAgent are unfinished.

## TLS options and activation

The [TLS contract](tls.md) applies to ca, rejectUnauthorized, minVersion/maxVersion,
ALPN, hostname/IP validation and remaining provider settings. ca replaces default
anchors; verification defaults to enabled. No process environment flag implicitly
disables checking. Options for unsupported TLS policies are forwarded to validation
and fail explicitly, rather than being silently discarded.

An omitted servername is derived from the request Host header, or target hostname
when that header is absent. IP targets suppress automatic SNI. Explicit servername
overrides that selection. DNS/IPv4/IPv6 and a custom Host header are tested with the
matching SANs; arbitrary normalization and enterprise trust configurations remain
unverified. Ambiguous legacy IPv4 spellings reject explicitly in TLS validation.

ALPN defaults to http/1.1; an empty protocol list can disable negotiation. Other
ALPN protocols reject explicitly because this module does not implement HTTP/2.
The request publishes socket before connecting, then activates writes only after
secureConnect. Tests confirm failed trust/hostname verification delivers no HTTP
request to the peer. Extra arguments to public http.ClientRequest cannot inject a
private HTTPS transport plan.

## Cancellation and lifecycle

signal is owned by ClientRequest for the complete upload/response lifetime. A
pre-aborted request never connects. Aborting in socket assignment also suppresses
connection; later abort destroys the real TLS transport with AbortError/ABORT_ERR
and the original reason as cause. Incomplete responses separately report aborted
and ECONNRESET. Paused reads, completed response cleanup, fatal errors and native
closure follow the supported HTTP/TLS contracts. Handle both request/body errors.

Request close can occur before a paused, already complete response emits end.
Consume the response and observe its end/complete state independently. The tests
wait for both events rather than assuming one fixed order. A transport that emits
error and close in one turn still reports the queued request error before request
close; response abortion is sequenced after that error.

## Limits and measured differences

- Server/createServer and Agent constructors reject with ERR_MITE_UNSUPPORTED_API.
  Client certificates, custom createConnection/verifier/context, proxy/local bind,
  sessions, upgrades, tunneling and HTTP/2 remain unavailable.
- Default anchors and strict SAN/close_notify policy differ from Node; consult TLS.
- A plaintext peer receives ClientHello, never an HTTP request. Node 24 wraps this
  failed negotiation as EPROTO; Mite preserves ERR_SSL_WRONG_VERSION_NUMBER. The
  independent wire test asserts both actual codes separately.
- Direct HTTPS helpers do not redirect or decompress; [global Fetch](fetch.md)
  supplies a separate body/redirect layer. HTTPS simply streams
  response bytes and status/headers. Complete Node option/error/platform parity is
  not claimed.

The suite checks HTTP/1 over both TLS versions, SNI/IP/SAN trust, encrypted framing,
HEAD/204, chunked trailers, uploads, one MiB paused data, URL/authentication, GC,
early/live cancellation, settled cleanup and fatal errors. Independent Python SSL
and plaintext peers check record selection, framing/truncation and refusal. See
[the example](../../examples/mite-https/client.mjs), [HTTP](http.md), [TLS](tls.md)
and [the architecture decision](../architecture/0080-https-private-transport-plans.md).
