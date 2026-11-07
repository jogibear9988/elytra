# 0111: Custom protocol origins before public registration

The actual Code-OSS entry reaches the missing Electron protocol export. Its pinned
mainImpl requests privileged custom schemes; later protocolMainService registers a
vscode-file file handler, denies direct file access and handles managed remote
resources. An exported registration list alone would not implement this behavior.
Keep protocol absent until real loader contracts exist.

Start with actual engine origin identity and trust, as an internal native component.
A standard custom application URL must retain its own scheme/host/port boundary;
do not rewrite it to file or HTTPS to get loading or secure-context behavior.
Implement process-local immutable native policy in C++, adjacent to LibURL's origin
and LibWebCommon's trust decisions. This avoids an extra FFI, reuses upstream's
Rust-backed URL/domain parser and does not alter Mite's standalone dependency graph.

Compose exactly two fingerprinted upstream translation units in our build tree.
Preserve upstream bytes outside the owned include/call seams and all notices. Guard
missing/duplicate seams, fingerprints and source-target ownership; review these
adapters on upstream updates. No upstream edits or patch stack is introduced.

Only a native caller can publish the complete validated table, once. Bound entries
and canonical ASCII names, refuse duplicates and built-in/internal scheme takeover,
and retain a fixed process-lifetime table. Atomic release/acquire publication and
immutable storage support worker reads without a reset/destruction race. A rejected
batch does not consume registration. There is no page/preload/configuration input.
Product and diagnostic helpers keep the empty default policy in this slice.

For standard registered URLs, require a hierarchical nonempty valid host; canonical
host parsing supplies case/IDNA/IPv6 identity. Preserve explicit custom ports,
including 0 and 443, rather than applying an HTTPS default. Paths/query/fragments
and userinfo do not become origin identity. Secure trust requires a registered
secure standard tuple origin. Opaque, invalid or unregistered URLs do not gain it.
Default upstream HTTPS/HTTP/loopback/file behavior remains intact.

The non-GUI probe executes actual composed URL::origin and LibWebCommon trust
functions, testing default refusal, scheme/host/port separation, relative URLs,
IDNA/IPv6, atomic invalid/duplicate/oversized policy refusal and concurrent reads.
This is not a mock or a test of a disconnected policy parser. Require the gate in
macOS release/check-firefly, Linux renderer and experimental native Windows CI.
Standalone CTest checks source composition only, not native protocol behavior.

Limitations are deliberate: the native origin hook does not implement full
Chromium custom-standard URL parsing/mutation, helper policy delivery, resources,
request handlers, fetch/CORS/CSP privileges, ServiceWorkers or V8 code caches.
Public protocol registration remains absent and the Code-OSS failure stays real.
Next implement parser semantics and trusted helper configuration, followed by the
bounded actual file/request transport and renderer loader gates; do not advertise
those missing parts from origin-component success.

See [native implementation](../../firefly/native/protocol/README.md),
[Code-OSS priority inventory](../compatibility/vscode.md),
[Electron protocol contract](https://www.electronjs.org/docs/latest/api/protocol)
and [upstream update process](../development/updating-ladybird.md).
