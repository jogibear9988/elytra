# 0077: TCP cancellation owns native transport shutdown

Extend the existing realm-local private abort hooks to node:net. Keep signal state
in the bootstrap adapter and native transports in the existing libuv service;
introduce no new engine/LibWeb dependency and no Ladybird modifications.

Socket constructors associate a signal after initializing Duplex and socket state.
Remove signal from the portable stream constructor options so its older private
AbortController dependency cannot install a second, public-event-based consumer.
Real abort destroys the socket with AbortError/ABORT_ERR and original reason cause.
Pre-aborted inputs defer delivery until the caller can attach error listeners.
The existing native close path cancels connect/read/write work, then completes
stream destruction. Late resolver results are rejected by destroyed/id checks.
An OS DNS request already running may still drain; do not claim native DNS abortion.

A connect factory owns its unreturned Socket if argument validation/startup throws;
destroy it before rethrowing to release signal/transport state. Direct
Socket.connect signal options stay explicitly unsupported; supply the signal at
construction, as the factory does.

Server.listen associates a signal with the allocated transport identity. Abort
calls ordinary close, stopping accepts while allowing established clients to drain.
Release its subscription on close or bind/lookup failure. Identity guards stop
stale/pre-aborted callbacks from closing a later listen generation. Defer numeric
binding to a nextTick turn, preserving asynchronous startup and allowing pre-abort
to close without binding/listening notification.

The new echo tests exposed an existing EventEmitter integration defect: once()
bypassed an overridden on(), so Readable.once('data') never resumed consumption.
Route once/prependOnceListener through their public hooks as Node does. Check
subclass hook identity/order separately and use actual socket pipe/once data flows.
A write waiting for connect reports ERR_SOCKET_CLOSED_BEFORE_CONNECTION; the socket
error event separately preserves the cancellation reason.

Thirteen actual Node 24.15.0/Mite cancellation scenarios cover established and
pending connections, buffered writes/late custom lookup, pre-abort, signal types,
propagation stops, accepted-client survival, manual-close reuse and failed-bind
reuse. Assert three observed differences explicitly: synthetic events do not
cancel Mite native consumers; pre-aborted unconnected sockets stay destroyed
(reconnect unsupported); a failed bind's stale signal cannot close a new listener
(Node 24 retains it until close). Do not replace those differences with fake equal
output. All local validation is non-GUI; Firefly is compiled only.

See the [TCP contract](../api/net.md), [cancellation globals](../api/abort.md) and
[original native ownership decision](0017-native-tcp.md). Broader filesystem,
HTTP/process/stream cancellation and TLS/HTTPS/fetch remain separate requirements.
