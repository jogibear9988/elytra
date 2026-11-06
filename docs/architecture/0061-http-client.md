# 0061: Native HTTP/1 client streams

Status: implemented; local/native acceptance recorded in PROGRESS.

## Decision

Extend the existing llhttp 9.3.1 adapter with explicit response/HEAD modes, bounded
status text, status-code/message events and the same rooted parser closure. Keep
request parsing as the default so server behavior is unchanged. llhttp owns framing
validation; JS does not implement a second HTTP grammar. Its
[callback contract](https://llhttp.org/#api) defines the HEAD body-skip return value.

Implement Node-shaped ClientRequest, request and get over the existing native TCP
Socket, OutgoingMessage and IncomingMessage streams. Support URL/options overloads,
fixed/chunked uploads, interim responses, trailers, EOF bodies and read pressure.
Each request owns one socket; close, abort, timeout and parse failures must release
that socket and report errors through the appropriate stream. Premature EOF after
headers aborts the response with ECONNRESET rather than adding a request parser
error. Callback exceptions retain the runtime's normal fatal-error semantics.

Use generated local identifiers with export aliases in builtin ESM shims, so public
names such as get cannot collide with the shim's private loader binding. Capture
the URL constructor through the module host rather than the mutable global.
No extra native dependency or Ladybird source change is introduced.

## Scope and differences

Use one connection per request, including when agent is omitted. Request pooling,
custom agents/transports, TLS/HTTPS/fetch, Unix sockets, AbortSignal, local binding,
CONNECT and upgrades remain explicit unsupported surfaces. Reject non-close
Connection headers. These are subsequent implementation requirements, not inferred
support from the existing server or DNS tests. Compare supported client behavior
with the [Node 24.15 public API](https://nodejs.org/download/release/v24.15.0/docs/api/http.html).

The parser may enqueue data from one TCP read before JS backpressure pauses the
socket. Individual responses are not assigned an arbitrary total payload cap;
applications must consume or destroy them and enforce their own payload/deadline
limits. Header limits and strict framing still apply. Full Agent/lifecycle/stream
option parity and Windows/Linux native acceptance require separate evidence.

## Validation

Use an independent Python raw TCP peer so Node and Mite are not tested exclusively
against their own HTTP implementation. Fragment headers/body/trailers, independently
decode fixed/chunked uploads and check status/headers/body and request lifecycle.
Cover interim/HEAD/204/EOF, malformed/oversized/ambiguous framing, truncated bodies,
paused 2 MiB streams with a digest, lookup/timeout/abort and fatal callbacks.
Compare argument error names/codes against Node and separately assert explicit
unsupported policies. Keep all prior server tests. The Firefly ESM migration
fixture also performs an actual main-process client/server exchange before loading
the same server URL in a separate renderer.
