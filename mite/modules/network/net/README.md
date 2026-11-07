# TCP implementation

The [builtin facade](../net.mjs) assembles one realm-local module. Applications
continue using `node:net`, `net` and the same public constructors/functions.
Internal ESM files are bundled by the pinned embedding generator; they do not
become public application module paths or replace the native transport.

| Source | Responsibility |
| --- | --- |
| [policy.mjs](policy.mjs) | Argument/error policy, address validation and branded AbortSignal helpers |
| [connection.mjs](connection.mjs) | Native handle setup, bounded family attempts, retry timers and generation checks |
| [socket.mjs](socket.mjs) | Socket/Duplex state, byte pressure, connect/read/write/close, timeout and signal lifetime |
| [server.mjs](server.mjs) | Listener state, accepted connections, close/drain and listener signal lifetime |

The socket factory owns its private WeakMap. The connection-attempt policy uses
that same map and returns native events to the socket dispatcher; stale callbacks
must match the current handle generation. The server factory owns a separate
WeakMap and adopts accepted handles through a private function from the socket
factory, preserving a single public Socket class and lifecycle implementation.

Native operations remain on the existing Mite event loop. Release signals and
pending timers at shutdown, retain close/error ordering and preserve backpressure.
The facade exports no state maps, native handle adopter or alternative transport
selector. New unsupported modes must keep failing explicitly.
