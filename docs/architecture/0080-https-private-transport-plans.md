# 0080: HTTPS uses private transport plans and shared HTTP classes

Keep one authoritative HTTP client/parser/response implementation. A frozen private
plan supplies protocol, default port, socket construction and readiness event.
Ordinary http requests use TCP/connect; HTTPS uses a verified TLSSocket/secureConnect.
Public ClientRequest extra arguments cannot select a plan without the factory's
opaque token. Preserve existing Node request/response class identities.

ModuleLoader roots separate HTTP/TLS constructor hooks and supplies them only to
trusted HTTPS bootstrap factories. Hooks are absent from public builtin exports and
globals. TLS's private constructor creates an unconnected client socket; it does
not change the public unsupported direct-wrapping policy. HTTP can publish socket,
subscribe/cancel, then connect only if the request is still alive. Request-level
abort owns the whole response lifetime, so do not install a duplicate TLS consumer.

HTTPS defaults to port 443, derives SNI from Host/hostname while suppressing IP SNI,
and offers only HTTP/1.1 ALPN. Strip HTTP-owned path/framing/signal fields before
TLS construction; forward provider policy fields so unsupported values reject.
Native TLS must validate before activating HTTP writes. No protocol fallback to
plaintext is permitted. TLS also rejects noncanonical numeric IP spellings before
DNS/SAN selection; WHATWG normalization recognizes aliases that strict isIP does
not classify as literals.

A new trust-failure test exposed an HTTP lifecycle issue: a stream can emit error
and close in one tick while request.destroy has queued its own error for the next
tick. Track the pending request error without adding a listener that would swallow
unhandled errors. Defer close/response abortion behind that queued error. Keep
one-shot close/failure guards and fatal-error behavior. A paused complete response
may outlive native request close; test both completion barriers.

Node/Mite clients use independent Node HTTPS servers for certificate failures,
SNI/IP/IPv6, upload/response framing, trailers, pressure, signals and GC. Separate
Python peers inspect ClientHello before TLS wrapping and test decrypted chunked/
truncated bodies. A plaintext server gets no HTTP request; preserve measured Node
EPROTO vs Mite OpenSSL reason codes rather than normalize them into equality.
Provider policy/default/class identity/fatal error tests remain explicit. Windows
can run native acceptance separately if the behavioral reference crashes, while
keeping the failing comparison gate visible.

This is HTTPS client support; Agents, TLS/HTTPS servers, HTTP/2 and fetch remain
unfinished. Keep Ladybird unmodified and build this runtime in standalone Mite and
Firefly. Local acceptance uses headless tests/builds, not repeated desktop launches.
See [the HTTPS contract](../api/https.md).
