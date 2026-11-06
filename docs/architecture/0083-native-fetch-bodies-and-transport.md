# 0083: Native Fetch objects and incremental HTTP exchanges

Status: accepted for a partial server-side runtime slice.

Build authored ESM Headers, body, Request/Response and exchange modules under
runtime/bootstrap/fetch. Generate a private embedding factory; there is no invented
node:fetch module. Install the four global exports after native URL/encoding and
portable stream setup. The loader supplies only trusted private cancellation,
constructor/disturbance hooks and lazy HTTP/HTTPS/stream module resolution.
Headers/Response-only programs need no transport modules or sockets. No Ladybird
changes, Node/V8/LibWeb runtime or new package is required.

Headers own normalized lists and response guards. Preserve separate Set-Cookie
entries and Node's Cookie separator. Body extraction snapshots byte/text inputs;
stream bodies remain incremental. Convenience consumers collect explicitly, while
direct reads/clone/cancel observe the portable stream's real disturbed state. Keep
the pinned provider's _disturbed detail in one private bootstrap hook. Clone uses
tee; Request construction transfers body responsibility. Do not advertise worker/
cross-realm integrity or resistance to application mutation of provider internals.

Each fetch exchange owns request upload, response stream, private signal observation
and terminal disposal. Adapt HTTP responses through a finite byte-counted Node/Web
queue and a BYOB-capable byte stream. Respond with zero bytes to pending BYOB EOF;
closing alone left a read unsettled and was caught by the completion-marker test.
Resolve at headers, preserve HTTP failure responses, wrap network failures with
their causes and preserve explicit abort reasons. Release the one-use connection
and cancel pending producers when a response completes early. Node reference
differences for suppressed public abort events and pending-pull producer shutdown
are asserted explicitly rather than hidden.

Follow up to 20 redirects with method/body rewrites, replay only snapshotted sources
and strip credentials across origins. Dispose old exchanges before following.
Require native verified HTTPS and reject nonidentity Content-Encoding instead of
returning corrupt decoded bodies. Add independent HTTP barriers/upload byte checks,
peer-observed cancellation/truncation, TLS ClientHello/trust refusal and a manual
system-trust HTTPS probe. Positive HTTPS hosted/default-trust platform coverage
still needs expansion beyond existing native TLS/HTTPS suites.

This is not full Fetch: compression, Blob/FormData, integrity/cache/referrer policy,
pooling/proxy/custom trust configuration and full WebIDL/WPT/security/GC coverage
remain open. Unsupported modes reject explicitly. Build Firefly but keep local GUI
acceptance infrequent/batched. See [the Fetch contract](../api/fetch.md).
