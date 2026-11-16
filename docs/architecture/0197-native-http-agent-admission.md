# ADR 0197: HTTP Agent connection admission

Status: implemented; native validation recorded in PROGRESS.

## Demand

The unmodified Code-OSS `agent-base` dependency subclasses `http.Agent`, performs
asynchronous connection setup, sometimes delegates to another Agent and borrows
`https.Agent.prototype.getName`. An empty constructor would permit evaluation but
cannot run its real requests. The existing HTTP parser already handles streaming,
framing, errors and cancellation; a second HTTP client would duplicate that work.

## Decision

Keep Agent admission in maintained ESM, bundled into the existing LibJS builtin.
Use the existing native TCP/TLS streams and llhttp parser. Agent owns connection
admission; ClientRequest adopts one socket and owns HTTP framing. HTTPS inherits
the same Agent lifecycle and selects the genuine TLS connector through private
embedding hooks. It requires a native TLS socket, not a public `encrypted` flag.
No Node/V8, LibWeb dependency or Ladybird modification is introduced.

Private records account for queued, connecting and assigned requests independently
of mutable public socket arrays. Reserve before invoking an overridable connector.
Release assigned credit only on real socket closure or explicit Agent removal.
Observe request socket publication when a subclass delegates to another Agent;
retire the outer reservation without taking ownership of the delegated socket.
Late/duplicate callbacks cannot revive requests or destroy another request's socket.

Defer socket publication so callers can install listeners. Never reconnect an
Agent-provided socket; activate HTTP only when its transport is ready. Preserve
queued cancellation and request error/close ordering when there is no socket yet.
Validate HTTPS options before admission without allocating a TLS engine; actual
socket construction repeats the same authoritative validation.

## Limits and remaining work

This slice uses `Connection: close`. There is no idle pool or socket reuse.
Keep-alive, TLS session caching and automatic environment proxy routing fail
explicitly; pooling and tunneled/wrapped TLS remain required future work rather
than successful proxy-support claims. Global Agents deliberately use no reuse.
Configured origin/total limits control admission without an invented hidden cap.
Destroy also cancels pending requests, a documented difference from Node's
socket-only Agent cleanup. Native platform and real application evidence must be
reported separately from source-hosted tests.

Reference: [Node HTTP Agent](https://nodejs.org/docs/latest-v24.x/api/http.html#class-httpagent),
the actual pinned Code-OSS dependency, and [HTTP API](../api/http.md#agent-admission-and-socket-ownership).
