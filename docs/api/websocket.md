# WebSocket

Status: partial standalone client API, validated locally on macOS ARM64 against
Node 24.15 and independent raw TCP/TLS peers. Fresh Linux/Windows acceptance is
pending. Mite supplies real socket connections through its own TCP, HTTP upgrade
and verified TLS implementations. Its private RFC 6455 engine is pinned MIT
`ws` 8.21.0; Mite does not embed Node, V8, Firefly or LibWeb.

```js
const socket = new WebSocket('ws://127.0.0.1:8080/echo', ['owned']);
socket.binaryType = 'arraybuffer';
socket.onopen = () => socket.send(new Uint8Array([0, 128, 255]));
socket.onmessage = ({data}) => {
  console.log(data);
  socket.close(1000, 'done');
};
socket.onerror = (event) => console.error(event.message);
```

This example requires a local WebSocket server accepting the `owned` subprotocol.
There is no WebSocket server constructor in this global API.

## Constructor and connection state

`new WebSocket(url[, protocols])` connects to `ws:` or `wss:`. HTTP/HTTPS URLs
normalize to WS/WSS. Invalid URLs, URL fragments, invalid protocol tokens and
duplicate protocol strings throw `DOMException` with name `SyntaxError` before
opening a connection. URL/protocol string-conversion exceptions propagate.
`protocols` can be one string or an iterable; the `{protocols}` constructor
options form is also accepted.

Instances inherit from the real standalone `EventTarget`. They expose readonly
`url`, `readyState`, `protocol`, `extensions` and `bufferedAmount`, plus writable
`binaryType`. Constants `CONNECTING=0`, `OPEN=1`, `CLOSING=2` and `CLOSED=3` are
available on the constructor and prototype. Negotiated `protocol` reflects the
server handshake. Compression is disabled, so successful connections have an
empty `extensions` value.

WSS uses the same certificate/hostname verification and trust policy as
[node:tls](tls.md). `NODE_EXTRA_CA_CERTS` can add a startup-loaded PEM CA; it does
not disable verification. The independent acceptance peer checks both an
untrusted connection that sends no HTTP request and a CA-trusted connection
that exchanges real WebSocket frames.

## Events

`open`, `message`, `error` and `close` support both `addEventListener` /
`removeEventListener` and `onopen`, `onmessage`, `onerror`, `onclose` properties.
An error event carries `error` and `message` details. A close event carries
`code`, `reason` and `wasClean`; clean status requires an actual close-frame
exchange. Abrupt transport loss reports code 1006 with `wasClean=false`.

Global `MessageEvent` and `CloseEvent` inherit from `Event` and use branded,
readonly payload getters. `MessageEvent` defaults are `data=null`, empty
`origin`/`lastEventId`, `source=null` and empty `ports`; its supplied `data` is
retained by identity. `CloseEvent` defaults are code 0, empty reason and
`wasClean=false`. Constructor dictionary getters are read once.

Synthetic message events cannot introduce port or source capabilities:
nonempty `ports` or non-null `source` throws `ERR_MITE_UNSUPPORTED_API`.
`MessageEvent.initMessageEvent`, full browser event/trust metadata and wider
WebIDL conformance are not implemented by this subset.

## Sending, receiving and pressure

`send(data)` accepts strings, ArrayBuffer, intrinsic typed-array/DataView slices
and Blob. Other values use string conversion; Symbol conversion throws.
Binary sends snapshot the supplied bytes and offsets before returning. Later
caller mutation does not alter accepted wire data. Blob reads preserve their
FIFO position relative to subsequent sends.

Incoming text is a string. Incoming binary defaults to Blob; setting
`binaryType='arraybuffer'` selects ArrayBuffer. Other string values leave the
current binary type unchanged. The engine validates server framing and UTF-8,
reassembles fragmented messages, automatically answers ping frames and masks
client frames with genuine cryptographic randomness.

Calling `send` while CONNECTING throws `InvalidStateError`. Calls after OPEN
has ended discard their captured payload and add its byte count to
`bufferedAmount`, subject to the same bounds. Outgoing admission uses private
buffer accounting, independent of an application-shadowed public property.

## Closing and resource limits

`close([code[, reason]])` starts a close handshake. An explicit code must be
1000 or in 3000–4999; otherwise it throws `InvalidAccessError`. The UTF-8 reason
must fit 123 bytes or `SyntaxError` is thrown. Repeated calls during/after close
do not start another handshake.

Mite drains accepted Blob reads and later queued sends before emitting Close.
This intentionally differs from the observed Node 24.15 behavior when closing
immediately after a Blob send: Node writes Close ahead of the pending Blob/tail
queue, while Mite sends its admitted FIFO work before Close. The independent Node
peer drains that exact queued suffix before replying, avoiding a TCP reset from
unread bytes; it still requires a clean close. Early close during CONNECTING
cancels the transport;
its focused regression requires no open/message event and prompt terminal close.
Node 24.15 can retain CLOSING after the corresponding close event; Mite requires
CLOSED. The focused early-close regression also passes on the local macOS native build.

| Resource | Current bound |
| --- | --- |
| Active connections per runtime/realm | 128 |
| Subprotocol entries | 64; reaching a 65th entry closes the iterator before admission |
| One sent or received message | 1 MiB; includes the assembled size of fragmented incoming data |
| Outgoing buffered bytes per connection | 4 MiB across pending Blob reads, engine writes and discarded sends |
| Pending facade send entries | 1024 |
| Handshake timeout | 10 seconds |
| Close-handshake timeout | 5 seconds |

Send, connection and protocol-list quota failures throw
`RangeError` / `ERR_MITE_WEBSOCKET_LIMIT`. Oversized or malformed incoming data
fails the connection; it is not delivered as a message. These are explicit Mite
limits, not promises of identical Node/browser quotas.

## Unsupported modes

Custom `dispatcher`, request `headers`, `agent`, `ca` and `rejectUnauthorized`
constructor options throw `ERR_MITE_UNSUPPORTED_API`. In particular, a proxy
agent's Undici dispatcher is not silently ignored: custom proxy/certificate
routing is not supported by this slice. Redirect following, permessage-deflate,
WebSocketStream, browser cookies/origin policy and a global server API remain
unsupported. The private `ws` class and its Node-specific methods are not public
exports; its optional native addons are disabled in the pinned build.

## Validation

The maintained runner is `tooling/tests/test_websocket.py --mite <mite>`; Node
reference-only execution uses `--reference-only`. The independent peer validates
nonce/accept/subprotocol handling, coalesced 101 headers plus first frame,
masked client bytes, sliced-buffer snapshots, Blob ordering, fragmented text
with interleaved ping/pong, binary output modes, close details, invalid framing,
invalid UTF-8 and real TLS trust outcomes. Mite-specific controls cover message
limits, private buffered-byte admission despite a poisoned public getter and
explicit dispatcher refusal. A separate pure fixture checks event brands,
getter counts, conversion errors and bounded iterator cleanup.

Local evidence is recorded in `websocket-native-controls.log`,
`websocket-event-controls.log` and `websocket-send-bound-controls.log` under the
active build directory. The complete runner also passes with the rebuilt Firefly
binary in `ELECTRON_RUN_AS_NODE=1` mode (`websocket-embedded-controls.log`),
including prompt CONNECTING cancellation. This is a real embedded Mite runtime
check without a GUI window. These local results do not establish hosted-platform,
proxy routing, full WHATWG conformance or real Code-OSS extension activation.

The send-pressure control uses queued immutable Blob reads to fill exactly 4 MiB
before asynchronous draining. A loop of direct socket writes is not a reliable
queue probe: the OS may consume them synchronously. The reference Blob-close
order is also visible in Node's pinned
[connection](https://github.com/nodejs/node/blob/v24.22.3/deps/undici/src/lib/web/websocket/connection.js#L254)
and [sender](https://github.com/nodejs/node/blob/v24.22.3/deps/undici/src/lib/web/websocket/sender.js#L55)
implementations; fresh Windows execution remains a hosted acceptance gate.
