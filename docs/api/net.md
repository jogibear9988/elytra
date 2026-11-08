# node:net

Status: partial native IPv4/IPv6 TCP and local stream implementation, using Mite's libuv loop and
portable Duplex streams. This is independent of Firefly and LibWeb. Local acceptance
is macOS ARM64; hosted platform gates remain required for each changed slice. See
[the compatibility matrix](../compatibility/mite-0.1.md) for platform evidence.

```js
import { createServer } from 'node:net';

const lifetime = new AbortController();
const server = createServer((socket) => {
  socket.on('error', (error) => console.error(error.code));
  socket.pipe(socket);
});
server.on('error', (error) => console.error(error.code));
server.listen({ port: 3000, host: '127.0.0.1', signal: lifetime.signal });
// Call lifetime.abort() to stop accepting; existing connections finish normally.
```

## Exports and construction

Default and named ESM exports share the CommonJS builtin instance. Exports are
Socket, Server, createServer, connect, createConnection, isIP, isIPv4 and isIPv6.
connect and createConnection are the same factory. isIP returns 0, 4 or 6;
isIPv4/isIPv6 return booleans. DNS names are not numeric addresses.

`new Socket([options])` creates a Duplex without connecting. Supported options
include allowHalfOpen, noDelay, keepAlive, keepAliveInitialDelay, ordinary byte
stream watermarks and signal. Object-mode sockets are rejected. A socket's signal
is associated at construction, including when using createConnection(options).
To use a separate Socket.connect call, supply signal to the constructor.

`createServer([options][, connectionListener])` creates a Server/EventEmitter.
Options include allowHalfOpen, pauseOnConnect, noDelay, keepAlive,
keepAliveInitialDelay and byte stream settings for accepted sockets. Its connection
listener receives a Socket. signal belongs in listen options, not createServer.

## Connecting

`connect(options[, callback])` / `createConnection(options[, callback])` return a
new Socket. `socket.connect(options[, callback])` or
`socket.connect(port[, host][, callback])` and
`socket.connect(path[, callback])` return the existing Socket. The callback
is a one-time connect listener. Attach an error listener before asynchronous work.

| Option | Supported behavior |
| --- | --- |
| path | Local Unix socket path or Windows named-pipe name; bypasses DNS/TCP |
| port | Numeric or numeric-string TCP port, 0–65535 |
| host | Numeric address or hostname; default localhost |
| family | 0, 4 or 6; IPv4/IPv6 string aliases supported |
| lookup | Custom callback resolver; invalid/duplicate results are checked |
| hints | Resolver hints passed to node:dns lookup |
| autoSelectFamily | Partial alternating-family attempts; false requests one result |
| autoSelectFamilyAttemptTimeout | Positive integer; minimum effective 10 ms |
| signal | Socket constructor/factory cancellation; realm-local AbortSignal |
| timeout | Factory convenience for socket.setTimeout |

A Socket exposes connecting, pending, readyState, readable/writable state,
bytesRead/bytesWritten and bufferSize. address() returns local metadata; localAddress,
localPort, localFamily, remoteAddress, remotePort and remoteFamily expose endpoints.
Metadata may be unavailable before connection or after close.

Events include lookup, connectionAttempt, connectionAttemptFailed,
connectionAttemptTimeout, connect, ready, data, end, drain, timeout, error and close.
Accept/connect order across different sockets is not fixed. Observe close to know
native teardown has completed. A failed connection can report a system error or
an aggregate of failed attempts; full Happy Eyeballs equivalence is not claimed.

## Local streams

Use `createConnection(path[, callback])` or `createConnection({ path })` to connect,
and `server.listen(path[, backlog][, callback])` or `server.listen({ path, backlog,
signal })` to listen. Paths use native libuv pipes with descriptor passing disabled.
Negative numeric strings are path names; nonnegative numeric strings retain the
TCP port overload. Client `path` takes precedence over `port`. On listeners,
a numeric/string `port` selects TCP first; explicitly supplied null/undefined
ports select TCP port 0. A numeric-only `path` without a port rejects rather than
binding an unexpected local name.

Local sockets share Duplex byte streaming, write-before-connect, backpressure,
half-close, timeouts, ref/unref, cancellation and close waiting. DNS lookup and TCP
connection-attempt events do not run. `setNoDelay` and `setKeepAlive` return the
Socket and perform no TCP operation. Socket address metadata is an empty object;
IP/port/family properties are undefined. A Server retains its original path from
`address()`, including after close or a failed bind, matching the Node reference.

Existing socket names and ordinary files are never automatically removed on a
failed bind. A successfully owned Unix listener is cleaned up by libuv on close.
`EADDRINUSE`, `ENOENT` and stale-endpoint `ECONNREFUSED` remain native errors. An
application may explicitly inspect and unlink its stale endpoint before retrying;
Mite does not make that ownership decision. Native permissions follow the process
umask or Windows default security descriptor. `readableAll`/`writableAll` are
explicitly unsupported.

Paths must be nonempty strings without NUL or unpaired UTF-16 surrogates. Abstract
Unix socket names are unsupported. Names are not silently truncated to fit a
native address. Mite additionally caps encoded paths at 32 KiB and shares a
4096-handle budget between TCP and local streams, including closing handles.
Exhaustion fails with ENOSPC; capacity returns only after native close. Platform
limits and permission errors still apply. Native Windows
and Linux acceptance for this new transport remains required; the implementation
alone is not platform acceptance. See [the architecture decision](../architecture/0177-local-stream-endpoints.md).

## Streams, options and closure

Socket implements the supported Duplex surface: write/end, read/push, pause/resume,
pipe, encoding, corking and backpressure. Check write's boolean result and wait for
drain only when it returns false. once('data', listener) starts readable flow as
on('data', listener) does. Native writes own copied bytes until completion.

| Method | Contract |
| --- | --- |
| destroy([error]) | Cancel pending transport work and close; close follows native teardown |
| destroySoon() | Finish buffered writes then destroy |
| setTimeout(ms[, callback]) | Notify inactivity; does not automatically destroy; timer is unreferenced |
| setNoDelay([enabled = true]) | Set/store TCP_NODELAY; return Socket |
| setKeepAlive([enabled = false[, initialDelay = 0]]) | Set/store native keep-alive; return Socket |
| ref() / unref() | Change native handle liveness; return Socket |

Normal end half-closes the write direction. allowHalfOpen controls whether a
received EOF automatically ends the writable side. Native callbacks run on the VM
owner thread with normal tick/Promise checkpoints.

## Listening

`server.listen(options[, callback])` and
`server.listen(port[, host[, backlog]][, callback])` return Server. Options include
port (default 0), host (default ::), backlog (default 511), ipv6Only and signal.
Listen startup and listening notification are asynchronous. The callback is a
one-time listening listener. For an assigned ephemeral port, inspect address()
after listening; TCP returns null before startup or after shutdown. Local listeners
retain the supplied path as described above.

`server.close([callback])` stops accepting and waits for accepted sockets to close.
The close event/callback is therefore delayed by active clients. Calling close
while not running reports ERR_SERVER_NOT_RUNNING to its callback.
getConnections(callback) asynchronously reports the current accepted count.
ref/unref change listener liveness; Symbol.asyncDispose closes a listening server.
A closed server may listen again after close completes. Listening while another
close is pending is explicitly unsupported.

## AbortSignal ownership

Socket abort calls destroy with an Error named AbortError, code ABORT_ERR, cause
equal to the original signal reason. A pre-aborted constructor defers error delivery
until the caller can attach listeners. Abort closes an established connection or
cancels connecting work. A late DNS completion cannot reconnect it. An already
running OS resolver request may still drain internally; its result is discarded.
A write waiting for connect reports ERR_SOCKET_CLOSED_BEFORE_CONNECTION separately
from the Socket's abort error.

Server listen abort behaves like close, emits no AbortError, and preserves accepted
connections. Pre-aborted or immediately aborted startup emits no listening event.
Subscriptions are released on Socket destruction, Server close and failed listen.
A failed connection factory disposes its unreturned Socket. Each listen subscription
is guarded by its transport identity, so a stale signal cannot close a new listener.

Signals must be Mite realm-local signals; foreign/polyfill objects reject with
ERR_INVALID_ARG_TYPE. Socket constructor falsy signals are ignored as in Node;
listen requires a valid signal whenever the option is supplied. Private abort
algorithms cannot be suppressed with stopImmediatePropagation. Synthetic public
abort events do not close native resources; actual controller abort still does.

## Differences and limits

- Direct Socket.connect({ signal }) and createServer({ signal }) remain explicitly
  unsupported. Associate signals at Socket construction or Server.listen.
- Node 24.15.0 resets destroyed state for a pre-aborted, never-connected Socket;
  Mite leaves it destroyed. Reconnecting destroyed sockets remains unsupported.
- Node 24.15.0 retains a failed bind's signal and can let it close a subsequent
  listener. Mite releases it on failure. The test asserts both results separately.
- Synthetic abort event cancellation differs intentionally, as described in the
  [cancellation contract](abort.md). Full event ordering/text/stack parity is absent.
- Descriptor adoption/passing, abstract Unix sockets, local TCP bind options,
  BlockList, reusePort, resetAndDestroy and connection-limit/drop policies are absent.
  TLS has a separate partial [client implementation](tls.md).
- Final connection attempts rely on OS deadlines; default IPv6 listen lacks a full
  IPv4 fallback policy. Native byte snapshots add copying overhead. Complete
  accepted-socket option and all platform/kernel failure combinations are unverified.

References: [Node net API](https://nodejs.org/api/net.html),
[async resource lifetime](../concepts/async-and-lifetime.md),
[TCP example](../../examples/mite-tcp/echo.mjs) and
[bounded cancellation example](../../examples/mite-tcp/cancellation.mjs).
