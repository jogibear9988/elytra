# HTTP implementation

The [builtin facade](../http.mjs) assembles one instance for a runtime/realm.
Applications keep importing `node:http` or requiring `http`; these files are
private implementation modules bundled by the checked embedding generator.

| Source | Responsibility |
| --- | --- |
| [protocol.mjs](protocol.mjs) | Token/header validation, duplicate-field policy, metadata and header serialization |
| [messages.mjs](messages.mjs) | Shared stream classes, response framing and private incoming/outgoing state |
| [client.mjs](client.mjs) | Request options, transport creation, response parsing, cancellation and socket teardown |
| [server.mjs](server.mjs) | Connection/parser ownership, response ordering, deadlines and close/drain |

The message factory owns shared WeakMaps; the client/server factories each own
their separate connection bookkeeping. The facade exposes the existing public
constructors and functions only. HTTPS receives the client transport selector
through a private embedding hook, preserving the shared message-class identity.
No helper state or arbitrary transport selector is exported to application code.

Native llhttp parsing and TCP/TLS I/O remain in the C++ adapters. Native callbacks
return through the owning Mite event loop; these modules coordinate streams and
translate errors without introducing a second runtime or dependency on Firefly.
