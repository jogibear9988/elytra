# ADR 0134: Simple CORS fetch over owned custom-protocol file channels

Status: accepted for the bounded custom-origin Window subset.

## Context

The prepared actual Code-OSS entry stops at corsEnabled. Accepting the boolean
without native origin checks and Fetch response filtering would misrepresent the
capability and expose file responses across origins. The existing native document
channel already supplies exact document identity and bounded streaming responses.

## Decision

Publish an optional boolean cors in the immutable native/helper schema-1 origin
snapshot; absent means false, nonstandard CORS is refused. Keep the Rust URL tuple
origin/trust rules unchanged. Classify only opted-in targets at the owned Fetch
composition seam; dispatch their file response through the HTTP-network entry so
upstream main_fetch, CORS checks, filtering and CSP still run. Preserve the pinned
upstream file byte-for-byte through reversible fingerprinted source composition.

Admit only active top-level custom-origin Window clients and simple GET/HEAD in
CORS mode without include credentials, preflight or Range. Both source and target
need main-owned default-session file routes. The renderer never receives path
selection authority. Main derives the source Origin from its bound document URL,
selects the target route's generation and exposes only that Origin header to the
handler. Require exact-origin or wildcard response authorization. Reject invalid
CORS metadata before granting body credit, cancel only that job and preserve the
well-formed document channel. Upstream performs its CORS/filtering checks as well.

Track used target schemes on each native document channel. Removing a source or
used target route retires the entire channel and all its pending requests. Stale
callbacks cannot revive it or a replacement generation. Reload grants a fresh
channel. This conservative whole-channel revocation is an intentional limitation.

## Acceptance and limits

Non-GUI actual LibWeb fetch tests verify 4-MiB file bodies, HEAD, CORS response type,
exact/wildcard origins, exposed/hidden/forbidden headers, rejected missing/mismatched/
duplicate authorization, zero unauthorized body credit, credentials/mode/preflight/
method/range refusals and job cleanup. A public hidden-window batch uses ordinary
main handlers and the production helper, including native source Origin delivery,
CSP refusal, channel survival after policy denial and target revocation/reload.

Local macOS Release is the accepted platform; fresh Linux and Windows desktop
acceptance remain open. HTTP/file/opaque sources, credentialed CORS, arbitrary
headers, preflight, workers/frames, general Response handlers, service workers and
code caches are not supplied. At this Ladybird pin CSP host matching refuses
custom opaque hosts; explicit custom-scheme sources work and remain subject to
native URL/path authorization. See the detailed [protocol contract](../api/protocol.md).

The real prepared Code-OSS entry now passes corsEnabled and fails at the next
unsupported privilege allowServiceWorkers (exit 1). This is measured startup
progress, not a workbench launch or proof of Code-OSS webview compatibility.
