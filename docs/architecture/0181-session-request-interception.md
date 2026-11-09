# ADR 0181: Authenticated session request decisions

Status: implementation with scoped native acceptance; see PROGRESS for results.

The pinned Code-OSS configureSession installs security-relevant request filters.
Accepting registrations without intercepting real Fetch traffic would let startup
advance while discarding those decisions. The implementation therefore suspends
actual requests and response heads and preserves independent file/protocol grants.

The main ESM policy implements one listener per stage, bounded URL filters,
callback decisions and generation revocation. Canonical frame proxies identify
native documents before preload IPC. Invalid, expired or obsolete continuations
fail closed. Main net.fetch/net.request enter the same policy on every transport
hop; their metadata has no invented renderer owner.

Renderer Fetch uses owned, fingerprinted source compositions after the upstream
request security checks and before get_response, plus a response-head continuation
before HTTP CORS. Owned custom/file responses defer their own head/CORS/body-credit
transition through the same gate. Cancellation composes the existing controller
path without replacing caller-owned stop steps. Completion retains the original
stream and native transfer lease; it does not buffer the response body.

One private byte channel per actual helper uses a random 128-bit bootstrap cookie
and actual PID. A typed versioned envelope carries process-local sequence,
generation and canonical page/navigable/document IDs. Native UI admission and
completion independently revalidate active ownership and top-level navigation
worker identity. The latter executes inside an owned LibWebView adapter because
upstream's CanonicalNavigation::worker is not a public exported symbol. No
upstream source or endpoint header is modified.

Stage tokens and logical request IDs are separate. Logical identity follows the
weak native FetchController through head processing and redirects; EOF, abort or
GC sweep retires it. Main net IDs use even values and native IDs odd values. Both
are monotonic. Pending requests, peer counts, payload bytes, header counts and
deadlines are bounded; helpers receive startup policy before VM initialization.
Changes to existing helpers use ordered asynchronous policy messages and are not
a synchronous distributed barrier.

The private codec is linked independently into the helper and UI. It depends on
AK/LibHTTP, avoiding a LibIPC-to-LibHTTP dependency cycle and avoiding an upstream
global header rebuild. Byte-valued HTTP header validation and immutable framing
checks run in both peers. A native probe covers every kind/stage flag combination;
this caught a contradictory header-stage condition that before-only tests missed.

Initial limitations are explicit: redirects and status-line overrides, other
webRequest stages, workers, unbound/subframe navigation and exact wire header
casing are unfinished. Unsupported renderer scopes fail closed under active
interception. Utility transports cannot yet forward policy decisions; native
mutual exclusion prevents live/new utilities bypassing installed filters. Replace
that restriction with authenticated parent forwarding to support Code-OSS utility
services. Mite's standalone node:http APIs remain independent of desktop Sessions.

Native schema/startup probes and controlled Node/Mite policy tests do not alone
accept interception. The public hidden/offscreen fixture must prove endpoint-zero
cancellation, CORS-changing header replacement, canonical cross-origin ownership,
stable IDs, main networking and custom-provider cancellation. Fresh platform,
packaging and real Code-OSS workbench acceptance remain separate requirements.
