# 0017: Native TCP on the shared libuv loop

Status: accepted for numeric IPv4/IPv6 TCP; macOS ARM64 validated.

## Decision

Use libuv's TCP handles on Mite's existing private loop. A C++ adapter performs
bind/listen/accept/connect, byte reads/writes, half-close, address queries and
socket options. JavaScript implements node:net's public facade using the pinned
portable Duplex implementation and Mite's EventEmitter. This stays entirely in
our repository; Ladybird, LibJS and the portable stream sources are unmodified.
No extra language/FFI boundary or new third-party dependency is introduced.

EventLoop exposes native loop access, guarded callback dispatch and shutdown
hooks to native services. Each network callback enters JS on the VM thread,
reports uncaught errors through the existing fatal path and performs a tick/job
checkpoint. During teardown hooks close all remaining handles; completions are
drained without application JS, then the loop and VM can be destroyed in order.

A transport owns handles by private monotonically increasing ids. A rooted JS
callback keeps each socket/server facade alive until native close completes.
Accepted handles receive their own callback before reading; failed accepts never
send a client-close event through the listener's callback. Write requests own
native byte snapshots and rooted completion callbacks until libuv completes or
cancels them. Request callbacks precede handle destruction. Reads allocate at
most 64 KiB, copy into engine byte storage, then release the native allocation.
No background thread touches the JS heap. Read backpressure stops native reads;
subsequent Duplex demand restarts them. EOF signals and processes the readable
end without consuming buffered bytes.

Server.close stops accepting immediately but waits for accepted sockets before
emitting close. Socket.destroy waits for native close, including pending request
cancellation. Destroy during connect completes a waiting write callback with
ERR_SOCKET_CLOSED_BEFORE_CONNECTION. ref/unref affect native liveness; teardown
also closes live unreferenced sockets. Timeouts notify without automatically
destroying a connection and do not keep an otherwise idle process alive.

## Evidence

The two network suites include 14 shared TCP workflows, 63 address/validation
comparisons, an independent Python OS-socket peer against both runtimes, fatal
native callback checks against both, and nine explicitly unsupported options.
Workflows cover IPv4/IPv6, Unicode echo, class/builtin identity, direct and corked
3 MiB transfers with backpressure/drain, half-close replies, delayed consumption, timeout notification, listener
conflicts/retry, connection refusal, server close waiting, pending write/connect
cancellation, live unreferenced handles and native callback queue ordering.
Tests synchronize independent accept/connect events rather than assuming their
relative order across kernels/runtimes. They compare application output with
Node v24.15.0. The external peer additionally proves byte transport without a
second copy of Mite masking a matching bug.

The transfer fixture waits for drain only when write returns false. A direct large
write can return true when the OS accepts it immediately, as observed on native
Windows Node; waiting unconditionally then hangs the reference itself. A separate
corked transfer forces the buffered-above-highWaterMark branch and requires false,
registering its drain listener before uncork. Both modes await the write callback,
check an empty write queue, received bytes/checksum/acknowledgement and bytesWritten.
No payload reduction or timeout extension is used. The runner executes both
runtimes even if one times out, keeping that timeout a failed comparison.

Readiness published through piped console.log is exercised by that external peer.
Console lines now flush immediately, rather than waiting for process exit or a
full C stdio buffer. This remains a synchronous minimal console, not Node stdio
stream support.

## Remaining scope and differences

This first TCP slice accepts numeric addresses only. Hostname resolution,
Happy Eyeballs, lookup/connection-attempt events, local binding and family/hints
options remain open. The current client default is explicitly IPv4 loopback
127.0.0.1 rather than DNS localhost; default listen binds :: without Node's IPv4
fallback. Use explicit numeric host values for the tested behavior.

Unix sockets/named pipes, descriptor adoption, AbortSignal, BlockList, reusePort,
resetAndDestroy, reconnecting a destroyed Socket and starting a new listen while
Server.close is pending reject with ERR_MITE_UNSUPPORTED_API. maxConnections /
drop policies, full error text/stack fidelity and all constructor/setter edge
cases remain unimplemented or unverified. bytesWritten currently counts writes
dispatched to native transport, excluding chunks still queued by Duplex; Buffer
mutation after native dispatch cannot change the copied pending write bytes.
Socket options have been exercised for valid inputs, not verified by OS-level
keepalive timing or packet inspection. Read/write errors reuse libuv status codes.
Internal stream scheduling differences from ADR 0016 still apply.

Handles remain rooted until explicit close or process teardown; automatic GC
closing of a forgotten native socket is not implemented. Force-GC behavior,
stress/resource exhaustion, native Windows and native Linux are unverified.
HTTP/TLS/fetch are still absent and this is not a general networking compatibility
claim. All native lifecycle paths require review as additional transports land.

References: [libuv TCP](https://docs.libuv.org/en/v1.x/tcp.html),
[libuv streams](https://docs.libuv.org/en/v1.x/stream.html),
[Node net reference](https://nodejs.org/api/net.html), and the observed Node
[v24.15.0 stream EOF handling](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/stream_base_commons.js).
