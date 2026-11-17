# ADR 0199: Standalone WebSocket clients over native transports

Status: implemented and locally tested on macOS ARM64; other platforms and
full Code-OSS acceptance remain pending.

## Context

The actual Code-OSS extension host reaches its proxy resolver, whose unchanged
`@vscode/proxy-agent` requires a genuine global WebSocket constructor and
prototype. Mite has no WebAssembly HTTP parser for direct Undici reuse. A fake
constructor would conceal the next transport failure and violate the runtime
contract. Mite must remain independent of Firefly, LibWeb, Node and V8.

## Decision

Reuse the exact npm `ws` 8.21.0 client engine privately, with lockfile integrity,
source hashes and its MIT notice verified before code generation. Optional
native addons are disabled by compile-time definitions, without changing process
environment. Expose no `ws` or engine builtin. The private dependency loader can
resolve only an explicit set of genuine Mite builtins. The authored ESM facade
provides WebSocket, MessageEvent and CloseEvent over the same EventTarget used
by the runtime globals. This implementation does not change Code-OSS or Ladybird.

Supply real HTTP 101 handoff with the existing native llhttp parser: detach owned
HTTP listeners, emit the exact unconsumed head once, and relinquish socket
ownership to the application. Support request createConnection and defaultPort
with actual Agent precedence and once-only socket adoption, rather than special
WebSocket exceptions. Preserve explicit CONNECT and pooling refusals. Server
upgrades remain unavailable. TLS trust and hostname verification use the native
client transport. Client handshakeTimeout metadata is ignored like Node's
TLS client; WebSocket owns an actual request timeout instead.

Add native crypto.randomFillSync and buffer.isUtf8. Native byte-source inspection
uses real internal slots, validates full backing-store ranges before access and
never invokes author getters or Proxy traps. Detached, shared and resizable
storage is explicitly unsupported. Keep these low-level operations in C++ next
to the existing entropy/text services; keep facade and payload ordering in ESM.

## Ownership and bounds

A connection owns its event target until native close, with at most 128 clients.
Limit each incoming/outgoing message to 1 MiB, outgoing buffering to 4 MiB and the
pending facade FIFO to 1,024 entries. Internal counters do not trust an overridden
public bufferedAmount getter. Snapshot BufferSource data before asynchronous
work. Immutable Blob reads retain their FIFO position, including before a later
close request. Aborting a connecting socket must cancel the real HTTP handshake;
it cannot wait for an open-state queue drain. Native sockets keep their ordinary
reference/lifecycle behavior; there is no artificial keepalive loop.

Disable compression and redirects; impose 10-second handshake and 5-second close
limits. Proxy/dispatcher, custom header and certificate-policy constructor options
fail explicitly until genuine delegation exists. Do not silently bypass policy
provided by Code-OSS's proxy patch. These limits and the Blob-before-close FIFO
difference from the observed Node reference are public compatibility differences.

## Evidence and remaining work

Independent Python raw TCP/TLS peers validate handshake bytes, masking, coalesced
101 head data, fragmentation, ping/pong, UTF-8 and invalid frame rejection,
subprotocol selection, payload snapshots, binary modes, FIFO, close/error states,
trusted WSS and rejection without HTTP bytes on an untrusted connection. Separate
Node/Mite controls cover HTTP connector/handoff and native byte primitives.
Event construction and queue-pressure tests cover public shadowing and bounded
admission. The tests execute actual standalone LibJS binaries, not a mock engine.

The offscreen Firefly build also succeeds. Actual Code-OSS startup and extension
activation, visible editing, proxy delegation, compression, WebSocket servers and
fresh native Linux/Windows validation remain separate acceptance gates. See the
[API contract](../api/websocket.md) and [progress log](../PROGRESS.md) for current
run-specific evidence.
