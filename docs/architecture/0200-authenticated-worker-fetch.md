# ADR 0200: Authenticated dedicated-worker fetches

Status: implemented partial renderer support; local macOS ARM64 native validation
passes. Fresh Windows/Linux desktop acceptance remains open.

## Demand

The original Code-OSS browser worker factory creates an application/javascript
Blob module containing an import of its actual worker entry. Session webRequest
listeners previously made that script fail: the owned Fetch gate rejected any
request from another agent thread. Removing the thread check would have skipped
policy enforcement, and letting a worker borrow a Window's GC objects or a
URL-matched protocol channel would have broken document authority.

## Decision

Compose pinned Ladybird service sources at native dedicated-worker admission,
agent entry/exit and WorkerHost lifecycle seams. Keep Ladybird unmodified. The
first creator must resolve to the actual hosted active document matching its
native environment ID. Bind its document/navigable/page identity and the exact
native document-channel epoch, obtained through Resources::Documents::client_for.
An inline page may claim its existing native grant lazily; no URL-only identity or
filesystem grant is created. Missing protocol authority preserves original worker
creation, but does not authorize an intercepted request.

A native Scope contains immutable IDs, generation, parent identity and weak event
loop references with atomic retirement. The creator document's GC weak reference
stays exclusively in the owner-thread registry. Each worker binds its actual
WorkerHost environment ID before the first fetch. Nested workers inherit the
native chain and must match their immediate creator environment. The focused
nested module/import/HTTP and parent-termination controls pass locally.

Worker FetchParams, pending responses and callbacks remain rooted on their own
agent. Cross-thread messages carry native metadata, bytes and numeric callback
keys, never GC cells. Owner posting resolves the live creator and current epoch
again; worker posting checks the exact attached scope. Termination, creator
replacement, binding refresh or shutdown revokes the scope. Retirement actively
rejects pending worker jobs, including descendants, rather than merely dropping
late callbacks and leaving their promises unresolved.

Worker policy requests enter the same owner FIFO as document requests, with the
existing 16 active decision slots. Before-request, send-header and response-header
decisions run through the real session handlers. HTTP uses the ordinary SDK
transport after admission; custom protocols use the existing authenticated native
streaming channel. The worker-local Response consumes actual chunks under body
credit and returns credit/cancellation through the owner relay. It does not fetch
an entire body into an intermediate JavaScript value.

For the Blob worker's main script, preserve the SDK Blob loader directly, including
its store, origin/partition, MIME and module validation. This matches the separate
Blob-loader branch in [Chromium 156's worker script loader](https://github.com/chromium/chromium/blob/156.0.8078.80/content/browser/worker_host/worker_script_fetcher.cc#L432),
which is the version pinned by [Electron 45.2.3](https://github.com/electron/electron/blob/v45.2.3/DEPS).
This exception is limited to the Blob main-worker destination; it does not exempt
worker HTTP or custom-protocol requests from interception.

## Bounds and partial scope

The owner Scope registry is bounded to 128 scopes per creator document and 1,024
per helper. Worker policy/protocol jobs share atomic helper-wide retention limits
of 32 MiB and 16,384 jobs; each worker has at most 8,192 logical request identities.
These bounds do not expand the owner broker's active decision window. Existing
native protocol URL/body, channel, timeout and framing limits still apply.

Custom-protocol worker fetches are same-origin GET/HEAD without Range, under the
creator's current authenticated channel. Cross-origin custom worker requests,
additional methods, shared/service workers, worklets and unbound worker scopes
are outside this accepted subset. Worker builtin-file interception has not been
accepted. There is no fabricated Window request, cross-thread VM access, custom
origin relaxation, Node/V8 embedding or change to standalone Mite worker_threads.

## Validation and remaining work

The real native Firefly/offscreen build passes. Existing protocol pending-response,
CORS/lifetime and 16 channel controls still pass. Six focused native renderer
groups pass: a genuine Blob module Worker imports an owned custom module and
performs HTTP Fetch; denied HTTP/custom requests reach neither actual transport;
and held decisions released after worker termination or owner navigation cannot
start stale transports. Additional cases validate asynchronous response-header
mutation, nested dedicated Blob worker imports/HTTP/header decisions and revocation
of a nested pending fetch when its parent terminates. A fresh worker barrier still
completes.

This evidence establishes the local renderer subset, not full Code-OSS worker or
editor acceptance. Next checks are fresh platform CI, wider worker
lifecycle/error/body cases and the original application worker workload. Unsupported
worker interception must keep failing explicitly until its native authority exists.
