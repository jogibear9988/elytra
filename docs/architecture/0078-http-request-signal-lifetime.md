# 0078: HTTP request cancellation spans upload and response

Associate private realm-local AbortSignal hooks with ClientRequest rather than its
Socket. The request owns HTTP lifecycle/error semantics; forwarding the signal to
the transport alone would lose the request error/reason or install two consumers.
Validate truthy signals before native allocation and preserve Node's ignored falsy
options. Keep Agent/TLS/other unsupported options explicit.

Install cancellation after request/parser/socket state and listeners exist, before
deferred socket association. Destroy a pre-aborted request synchronously: Writable
queues its error so callers can still subscribe before delivery, while destroyed is
already true on return. Normal abort destroys the request with AbortError/ABORT_ERR
and the original reason as cause; it does not invoke the legacy request.abort API.
A deferred socket turn checks destroyed before publishing/connecting. Existing TCP
identity/destroyed guards discard late DNS results. Running OS DNS work may drain.

Do not unsubscribe at upload finish. The same request can be waiting for headers,
streaming or paused on an incomplete response. Dispose during destruction, including
normal owned-socket closure. Forced-GC tests keep the controller alive and verify
completed request objects disappear after native close; settled requests do not
receive later abort errors.

Queue the request's error before initiating Socket.destroy. An unconnected Socket
queues close on nextTick, which otherwise could arrive before the request error.
Abort an incomplete response during native socket close: response aborted precedes
request close, and the queued response ECONNRESET/error/close follows. Parser/EOF
failure paths keep one-shot failure/close guards. When an unexpected native close
first creates the request error, defer request close behind that queued error.
Do not abort a complete response solely because its owned transport closes.

Tests compare actual Node 24.15.0 and Mite lifecycle sequences, default/URL options,
pre-abort and socket-association races, late lookup, completed upload vs response,
paused bodies, deadlines, propagation stops and subscription disposal. A separate
Python OS peer verifies EOF/reset after cancellation during a partial response and
an open large upload. Public synthetic abort events intentionally do not run Mite
private consumers; assert the actual Node difference separately. Inherited HTTP
Server.listen cancellation is accepted with an already accepted graceful response.
No local desktop/window tests are necessary; compile Firefly with the shared runtime
and leave changed-source platform acceptance to hosted gates.

This does not establish HTTP Agents, pooling, HTTPS/TLS/fetch or general stream,
filesystem/process cancellation. Keep the [HTTP contract](../api/http.md) and
[cancellation contract](../api/abort.md) explicit about that remaining work.
