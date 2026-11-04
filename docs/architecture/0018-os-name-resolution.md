# 0018: OS name resolution and hostname TCP connections

Status: accepted for lookup-based DNS/TCP integration; macOS ARM64 tested.

## Decision and ownership

Use asynchronous libuv getaddrinfo on the existing event loop for OS-backed
hostname lookup, including local host configuration. Implement the Node callback
and Promise lookup facades in our own JavaScript, plus builtin aliases and a small
C++ adapter. Reuse the existing LibURL IDNA helper for ASCII normalization. This
adds no dependency, FFI boundary or downstream Ladybird patch.

Each resolver request owns a rooted JS completion callback until the libuv
completion fires. Native addrinfo storage is converted to JS address/family
records on the VM thread and freed after dispatch; duplicate address strings are
removed while preserving OS order. No worker reads JS storage. Numeric literals
and empty-host compatibility paths complete through nextTick without submitting
getaddrinfo. Options support family selection, all, hints and result ordering;
callback-only IPv4/IPv6 family aliases are kept distinct from Promise validation.
Native requests also participate in EventLoop pending-work accounting, so fatal
errors with an outstanding OS request use the existing immediate-exit policy
rather than waiting indefinitely for an OS resolver. Normal requests keep the
loop alive. The shutdown hook attempts cancellation of queued work; running OS
resolution cannot be interrupted by libuv.

## TCP integration

net.Socket now accepts hostnames, family/hints, custom lookup functions and
bounded automatic address-family attempts. Its default host is localhost.
Resolution emits lookup events, deduplicates candidates, alternates families and
attempts one native socket at a time. Immediate failures move to the next address;
a timeout abandons a nonfinal attempt and tries the next. Successful attempts
clear the timer; terminal multiple failures carry an AggregateError. Attempt,
failure and timeout events and attempted-address metadata are provided. A fresh
native handle per retry avoids reusing a failed libuv TCP socket. Generation
checks ignore completions from replaced/closed handles and from a lookup that
finishes after Socket.destroy. Timers and native close completions retain the
existing ref/unref and GC ownership rules.

Server.listen can resolve a hostname before binding. Closing during resolution
closes the pending native handle and prevents a late callback from binding it.
The existing default listen path still lacks an IPv4 fallback on hosts without
IPv6; local bind/IPC transports and advanced policies remain open.

## Validation and limits

Six shared workflows cover public lookup/Promise/builtin identity, localhost and
numeric families, explicit/default result orders, native IDNA normalization to
numeric loopback, hostname/default-host client connections, hostname listeners,
custom synchronous/asynchronous lookup, bounded retry events, aggregate failures,
malformed/custom error results, cancel and closing during resolution. Another
119 comparisons cover DNS options and argument validation. Existing TCP suites
remain required.

A POSIX FIFO test occupies all four libuv workers, queues an actual OS lookup,
then requires a timer to run while that lookup is still unresolved. Independent
writers release the workers; the lookup and file reads must then finish. Fatal
callbacks and fatal main execution with pending resolver requests are exercised
against both runtimes. This test does not exercise native Windows worker behavior.
The suite uses local host configuration and numeric targets, not external DNS
servers or fragile Internet connectivity. Live remote DNS, resource exhaustion,
full timeout/error metadata, interface scope resolution and native Windows/Linux
remain unverified. Retry behavior is a partial implementation, not proof of
complete Node Happy Eyeballs parity or every connection race.

Embedded NUL in a hostname is deliberately rejected with ENOTFOUND before OS
submission. Node v24.15.0 instead resolves the prefix in the recorded numeric
fixture. Both outputs are independently asserted as a known difference; this is
not counted as a matching workflow. Empty-host compatibility remains available,
but Node's deprecation warning is omitted. Request-wrapper metadata/cancellation
is not exposed; destroying a socket ignores late resolution but cannot stop an
in-flight getaddrinfo. The last attempt relies on the OS connect deadline.

Only lookup-based DNS is implemented. Resolver, resolve* queries, lookupService,
reverse, server configuration, per-thread DNS settings and default family timing
getters/setters are absent. Core error names/codes are compared; error messages,
stack traces and all OS-specific resolver failures are not exhaustively matched.

References: [libuv DNS](https://docs.libuv.org/en/v1.x/dns.html),
[Node DNS](https://nodejs.org/docs/latest-v24.x/api/dns.html),
[Node net](https://nodejs.org/docs/latest-v24.x/api/net.html).
