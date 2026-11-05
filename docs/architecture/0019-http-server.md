# 0019: HTTP/1 server on native TCP and llhttp

Status: accepted for a partial HTTP/1 server; native macOS ARM64 tested.

## Decision and ownership

Link pinned llhttp 9.3.1 through the existing vcpkg baseline and exact override.
This is also the installed Node v24.15.0 reference's parser version. vcpkg pins
its source archive hash; our C++ adapter checks header version macros at compile
time because this package lacks a CMake version config. Retain the MIT notice in
vendor/licenses/llhttp-MIT.txt and with eventual binary distributions. Mite neither
links nor launches Node/V8. Ladybird remains an unmodified submodule.

C++ integrates llhttp directly with LibJS to avoid an extra Rust/FFI boundary.
A GC-owned parser cell captures incremental parser state in a private native
function. llhttp callbacks collect native events without entering JS. The adapter
then converts URL/header bytes as Latin-1, copies body spans into engine-owned
byte arrays and returns ordered begin/headers/body/trailers/complete events.
No input pointer survives execute. Parser/native container lifetime follows the
capturing JS function and the owning TCP connection. A failed connection stops
feeding its parser. Header budgets count URL/field/value bytes; the default is
16 KiB. Boundary accounting is not claimed identical to Node at every byte.

Our JavaScript facade supplies IncomingMessage on Readable, response writing on
Writable, and Server on net.Server. Strict native parsing handles framing,
duplicate lengths, CL/TE ambiguity, chunk syntax and truncation; no handwritten
HTTP body parser replaces it. Requests expose raw/merged/distinct headers and
trailers, version, method, URL and completion/aborted state. Socket read demand
and buffered-body completion resume native input after backpressure. Responses
support fixed and chunked bodies, trailers, status/header methods, HEAD/no-body
statuses and ordered pipelined output. Queued response writes become active only
after the previous response finishes. Unread request bodies are drained after
response completion. Close paths retain native write ownership until completion,
destroy idle connections and release listener liveness correctly.

The status-reason table is protocol data captured from the installed Node reference;
METHODS is the HTTP method map from llhttp, excluding RTSP/PRI entries. Runtime
versions.llhttp reports the linked parser version.

## Validation

Eleven shared TCP workflows compare raw responses/application metadata with Node:
GET and Unicode lengths, chunked POST/trailers/duplicate headers, chunked response
trailers and empty writes, ordered pipelining, HEAD/204, 100-continue, malformed
method/header/length/chunk/truncated requests, unread-body keep-alive, client FIN,
a delayed 512 KiB upload followed by connection reuse, default errors, and HTTP/1.0
close framing. Another 135 header/constant comparisons cover names/values,
arrays, mutation methods, header introspection, status/error guards, ESM identity,
METHODS, status reasons and parser version.

An independent Python HTTP client sends six requests over one connection to both
runtimes, including binary POST, streamed response, Date validation, HEAD and 204,
and requires server.close to finish. The public example's GET and piped binary
POST were also exercised with that client. These are application tests, not the
complete Node/llhttp upstream test suites or a production security audit. Fatal
request callbacks are checked against both runtimes as well.

## Deliberate policies and unfinished APIs

Mite rejects mutated header arrays containing CR/LF at serialization and refuses
to emit both Content-Length and Transfer-Encoding. The reference Node permits
these two recorded outgoing configurations; the header suite asserts both
independent outcomes rather than counting them as matching comparisons.

Headers/body deadlines use per-connection timers, and idle keep-alive closes at
keepAliveTimeout. Node's periodic timeout sweep and additional keep-alive buffer
are not reproduced. Mite policy fixtures exercise 408 deadlines and explicit 501
responses for upgrades/CONNECT. A clientError listener takes ownership of the
failed socket; the default path returns 400/431/408 as applicable and closes it.
Header-less HTTP/1.1 requests receive a normal 400 response without a request event.

This step implements a server only: ClientRequest/request/get, Agent/globalAgent,
HTTPS/TLS, fetch, upgrades, WebSockets and CONNECT tunneling remain absent.
insecureHTTPParser, uniqueHeaders, connectionsCheckingInterval,
rejectNonStandardBodyWrites and nonzero maxRequestsPerSocket reject explicitly.
Interim early hints are unsupported; exhaustive interim/status variants,
strictContentLength error timing, removeHeader suppression of automatic fields,
all constructor/timeout options and lifecycle race parity remain incomplete.
OutgoingMessage/ServerResponse use Writable internally, which differs from Node's
Stream-only ancestry. Only measured class identities are claimed.

Per-response stream backpressure and request input backpressure are exercised;
aggregate outgoing pressure across a long pipeline, pipelined request admission
limits and adversarial sustained-load memory behavior still need work. Native
execute handles at most a TCP read's current bytes, but can emit multiple message
events before JS callbacks run. Responses are therefore not a claim of complete
Node callback/microtask interleaving within a single packet. Node's separate chunk
extension budget and full error-position/message fidelity are not implemented.
Automatic GC/fault-injection/resource-exhaustion behavior and native Windows/Linux
remain unverified. Existing stream/network differences from ADRs 0016–0018 apply.

References: [llhttp](https://llhttp.org/),
[Node HTTP](https://nodejs.org/docs/latest-v24.x/api/http.html), and
[Node v24.15.0 server](https://github.com/nodejs/node/blob/v24.15.0/lib/_http_server.js).
