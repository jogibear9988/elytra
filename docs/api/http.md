# node:http

Partial HTTP/1 client/server support in Mite and the Firefly main process. The new
client is tested on macOS ARM64 and the native
Linux/Windows CLI HTTP suites at da5e3e8; [current evidence](../status.md) records
that baseline. Full option/error parity remains open. No renderer Node API is exposed. See the
[compatibility matrix](../compatibility/mite-0.1.md) for existing server/platform scope.

HTTP/2 and HTTP/3 are [planned later work](../roadmap.md#planned-http2-and-http3),
not implemented by this HTTP/1 module. They are below current platform priorities.

```js
import {request, get, ClientRequest, IncomingMessage} from 'node:http';
```

CommonJS require('http') and require('node:http') return the same module. The
default ESM export is that object; named exports preserve function/class identity.
The [Node 24.15 HTTP reference](https://nodejs.org/download/release/v24.15.0/docs/api/http.html)
is the public API baseline, not a claim that its entire surface is implemented.

## request(input[, options][, callback]) and get

input can be an HTTP URL string, URL object or options object. An additional
options object overrides URL-derived fields. request returns a writable
ClientRequest; call end() to finish sending it. get uses the same arguments and
calls end() automatically. callback receives one IncomingMessage on the response
event. It is optional; an unobserved response is drained automatically.

```js
import {get} from 'node:http';
const response = await new Promise((resolve, reject) => {
  get('http://127.0.0.1:3000/hello', {agent: false}, resolve)
    .on('error', reject);
});
response.setEncoding('utf8');
for await (const text of response) console.log(text);
```

Run the existing [server](../../examples/mite-http/server.mjs), then the
[client example](../../examples/mite-http/client.mjs), under Mite or Node.

| Option | Current behavior |
| --- | --- |
| protocol | http: only; other protocols throw ERR_INVALID_PROTOCOL |
| hostname / host | hostname takes precedence; default localhost; bracketed IPv6 URLs normalized |
| port / defaultPort | Explicit port wins; otherwise request defaultPort, Agent defaultPort, then transport default 80 (HTTP) or 443 (HTTPS); integer or numeric string from 0 through 65535 |
| method / path | Default GET and /; URL path includes query; token and unescaped-character validation |
| headers | Object or flat name/value array; common header accessors from OutgoingMessage |
| auth | Basic authentication string; explicit Authorization header takes precedence |
| setHost | false suppresses the generated Host header |
| agent | Agent-like object, false for direct transport, or omitted/null for globalAgent unless a callable request createConnection is supplied; no socket reuse |
| createConnection | Request-local native socket connector when agent is omitted/null; explicit Agent or false takes precedence |
| maxHeaderSize | Default 16384; zero selects that default; strict parser, bounded status/header fields |
| joinDuplicateHeaders | Enable joining singleton response headers; set-cookie stays an array |
| timeout | Socket inactivity timeout; emits timeout without automatically aborting |
| family / lookup / hints / autoSelectFamily / autoSelectFamilyAttemptTimeout | Forwarded to implemented native net.Socket connection logic |
| noDelay / keepAlive / keepAliveInitialDelay | Native TCP options; noDelay defaults to true |
| highWaterMark | Writable stream buffering option |
| signal | Realm-local AbortSignal; cancel request/transport, including after upload finish |

Invalid Agent values fail with ERR_INVALID_ARG_TYPE. socketPath, local binding,
uniqueHeaders, custom IncomingMessage and insecure parser mode throw
ERR_MITE_UNSUPPORTED_API. CONNECT remains unsupported. A non-close Connection
header is accepted only for an explicit, token-validated Upgrade request;
ordinary persistent HTTP connections remain unsupported.

A request-local createConnection(options, callback) may return a real connecting
or connected native socket, or supply one asynchronously with callback(error,
socket). It receives normalized host/port and transport options, with this equal
to that options object; the HTTP request path is removed from socket options.
The client adopts the socket once and never reconnects it. HTTPS requires a real
native TLS socket. Duplicate callbacks cannot attach another HTTP parser, and
late unowned sockets are closed after cancellation. An explicit Agent, including
agent:false, takes precedence over the request-local connector. defaultPort also
defines when the generated Host header omits the port suffix.

## Agent admission and socket ownership

`Agent`, `globalAgent` and the corresponding HTTPS constructors manage actual
connections. `new Agent({ maxSockets, maxTotalSockets })` limits simultaneous
connections per origin and across the Agent. Both limits default to Infinity.
Waiting requests appear in `requests`; admitted sockets appear in `sockets`.
`totalSocketCount` counts actual owned sockets. `freeSockets` remains empty.
Private admission records include asynchronous connectors, so public bookkeeping
changes made by subclasses cannot bypass the connection limits.

`addRequest(request, options)` calls the overridable
`createSocket(request, options, callback)`, which uses
`createConnection(options, callback)`. A connector can return a connecting or
already connected native socket, or supply it asynchronously through the callback.
The client adopts it once, publishes `socket` before activating HTTP writes and
does not connect it a second time. `getName(options)` supplies origin keys;
HTTPS keys also distinguish TLS settings. HTTPS Agent inherits from HTTP Agent.

Aborting a queued request removes it before socket admission. Late or duplicate
connector completions cannot revive a closed request. Actual socket closure
returns connection credit and admits the next waiting request; a client upgrade
also releases credit when ownership is handed off. `destroy()` closes
owned connections and cancels currently queued/pending requests; the Agent can
subsequently admit new requests. Cancelling pending requests is a deliberate
stronger cleanup policy than Node's socket-only Agent destruction.

This implementation does not reuse sockets: requests send `Connection: close`.
`keepAlive: true`, `reuseSocket` and automatic `proxyEnv` routing fail explicitly;
`keepSocketAlive` returns false. Unlike modern Node, Mite's `globalAgent` also
defaults to no reuse. Custom proxy negotiation, CONNECT and wrapped existing TLS
sockets remain separate work. A successful Agent import does not prove that a
proxy or VS Code extension can perform all its network operations.

Reference: [Node Agent API](https://nodejs.org/docs/latest-v24.x/api/http.html#class-httpagent).

Local macOS ARM64 validation compares six real loopback groups in Node 24.15 and
Mite: per-origin FIFO, cross-origin total limits, asynchronous/already-connected
subclasses, queued cancellation, delayed failure and teardown. Three additional
groups run the unchanged, SHA-256-checked `agent-base` 7.1.4 dependency from the
pinned Code-OSS stage, including genuine delegated Agents. The optional package
fixture requires `--stage`; ordinary CI explicitly reports it skipped when no
Code-OSS dependencies are staged. The ordinary Agent gate also passes in the
native [Linux](https://github.com/jogibear9988/elytra/actions/runs/43956881619)
and [Windows](https://github.com/jogibear9988/elytra/actions/runs/43956881539/job/133613751481)
CLI runs at f52fcb5; the overall Windows run still has separate reference/API
failures. The staged agent-base cases remain locally verified only. See
[ADR 0197](../architecture/0197-native-http-agent-admission.md).

## Client protocol upgrades

A successful HTTP/1 switching response emits upgrade(response, socket, head)
once instead of response. head contains exactly the bytes already received after
the switching headers, including binary bytes; the client does not feed them
back into the HTTP parser. The receiver owns subsequent socket reads and writes
and can use socket.unshift(head) before attaching its protocol reader.

The handoff removes only the client's own socket listeners and encoding guard,
preserves application listeners, and releases Agent ownership through agentRemove.
The request closes without closing the upgraded socket. Later destruction or
signal cancellation of that request does not destroy the handed-off transport.
Without an upgrade listener, the client closes the socket. A non-switching reply
to an Upgrade request remains an ordinary HTTP response. This is a generic client
handoff, not a WebSocket-only path; it does not add server upgrade handling,
CONNECT tunnels or connection pooling.

The focused runner `python3 tooling/tests/test_http_upgrade.py --mite <mite>`
compares seven groups against Node: coalesced binary head and duplex socket
ownership, split switching headers, non-101 rejection, unobserved upgrade cleanup,
connector return/callback and Agent precedence, late cancellation/error identity,
and real defaultPort selection. These shared cases pass in the Node 24.15
reference; platform acceptance for the new native client path must be recorded
separately from the older Linux/Windows HTTP baseline above.

## ClientRequest and streams

The request extends OutgoingMessage/Writable. Supported workflows include end(data),
write/end with chunked framing, and piping a Readable into the request. Explicit
Content-Length and chunked transfer encoding are supported independently; combining
them is rejected. Header names/values and request-line bytes are validated before
transmission, including mutable header-array values. flushHeaders is available.

Selected methods: setHeader, appendHeader, getHeader(s), getHeaderNames,
getRawHeaderNames, hasHeader, removeHeader, addTrailers, setTimeout, setNoDelay,
setSocketKeepAlive, destroy and abort. abort marks aborted and schedules the abort
event. destroy(error) reports the error and closes the owned HTTP socket; an
upgraded socket has already been handed off.
Inspect method, path, host, protocol, socket, res, finished, writableEnded,
writableFinished, destroyed and reusedSocket (always false).

Events covered by tests include socket, finish, response, information, continue,
timeout, error, upgrade and close. HTTP 100 emits continue and information; other interim
responses emit information. Response streams expose statusCode/statusMessage,
HTTP version, raw/merged/distinct headers, trailers, complete, aborted and req.
HEAD, 204, chunked and connection-delimited bodies are supported. Pausing a body
propagates read pressure to the TCP socket. Parsed bytes from the current TCP chunk
can already be buffered before that pause; no aggregate application payload cap is
imposed. Consumers must bound payloads and consume, resume or destroy responses.

TCP writes now attempt immediate completion before queuing native work. Empty
writes do not add a native completion when the socket has no queued bytes; public
write callbacks remain asynchronous. Completed-request finish/state observations
are compared with Node, including repeated immediate replies. A server can also
reply before an upload finishes: an early 413 response is tested without marking
the unsent request finished. Native Linux/Windows HTTP suites at da5e3e8 pass
these tested cases; broader error/option parity remains open. See
[ADR 0065](../architecture/0065-immediate-tcp-write-completion.md).

Parser failures retain llhttp error codes and metadata. A truncated body emits
aborted and ECONNRESET on the response with complete=false. Timeouts only notify;
call destroy to enforce an application deadline. Handle errors on both request and
response. Callback exceptions are fatal through the normal runtime error path.

## AbortSignal cancellation

`request` / `get` / `new ClientRequest` accept signal in options, including URL plus
options overloads. A real abort destroys the request with an Error named AbortError,
code ABORT_ERR and cause equal to the original reason. Before an upgrade handoff,
it closes the actual TCP connection. Request.aborted and the legacy abort event belong to request.abort();
signal cancellation does not set/emit them. After a successful upgrade, the
request no longer owns or cancels that socket.

A pre-aborted request is already destroyed when returned; its error/close events
are deferred so the caller can install listeners. It does not publish a socket or
send a request. Cancellation during deferred socket assignment or DNS lookup also
prevents activation; late resolver results cannot reconnect it. Already-running OS
DNS work may drain internally. The finish event only means the upload was sent:
the request remains abortable while waiting for or consuming the response.

For an incomplete response, cancellation emits request error, response aborted,
request close, then response ECONNRESET error and close in the tested sequence.
Handle errors on both request and response. A paused response also closes without
requiring further readable demand. Completed responses retain their complete body
state. Subscriptions are disposed during request destruction/normal transport
closure, not upload finish. A forced-GC regression confirms completed request
objects are collectable while their original controller stays reachable.

Signals must be from Mite's realm; foreign/polyfill signals reject with
ERR_INVALID_ARG_TYPE. Falsy signal options select no cancellation, matching the
Node reference. Public stopImmediatePropagation cannot suppress native cancellation.
A synthetic abort Event does not run private cancellation algorithms; Node 24
cancels on that event, an intentional difference shared with timers/TCP.

The inherited `Server.listen({ signal })` closes the listener on abort and allows
accepted HTTP requests/responses to finish under the existing server close policy.
It is tested with a graceful response after the listener was cancelled.

See the [cancellation example](../../examples/mite-http/cancellation.mjs),
[AbortSignal contract](abort.md) and [TCP ownership](net.md).

## Differences and remaining work

Default connections close after each response, unlike Node's reusable default
agent. Connection pooling, direct HTTP-helper redirects, CONNECT tunneling and
server-side upgrades remain absent. Global [Fetch](fetch.md) supplies its own redirect/body layer.
[node:https](https.md) now provides the shared client over verified native TLS;
node:http continues to reject https: inputs rather than changing transports. Error text, all event timing/option/header
parity and every stream edge case are not certified. The client uses the existing
strict llhttp parser and native TCP service, with no Node/V8 or LibWeb dependency.

The mite-http-client suite uses an independent Python TCP peer, comparing Node
and Mite for uploads, fragmented replies, duplicate headers/trailers, HEAD/204,
EOF/information, 2 MiB paused data, parser/overflow/ambiguous framing, truncation,
lookup failure, timeout, abort, lifecycle and argument validation. Separate cases
exercise unsupported option/CONNECT rejection and fatal callbacks; the separate
client-upgrade runner covers the switching handoff described above. Existing server
suites remain required. The HTTP cancellation suite additionally checks request/
response event order, startup/lookup/finish/paused/deadline cancellation, signal
validation, settled disposal and independent OS-peer closure during partial
response and large upload. See [ADR 0061](../architecture/0061-http-client.md).

## Server API

createServer, Server, ServerResponse, IncomingMessage, OutgoingMessage, METHODS,
STATUS_CODES, validateHeaderName/Value and maxHeaderSize remain available. The
[server guide](../guides/http-server.md),
[runtime reference](mite.md#tcp-dns-and-http) and
[server design](../architecture/0019-http-server.md) describe that partial surface.

Server-side protocol upgrades and CONNECT remain explicitly unsupported. The
client upgrade event does not change that server policy.
