# ADR 0175: Public Response handlers and native file interception

## Decision

Connect the asynchronous transport from ADR 0174 to the main-process protocol API.
Legacy file callbacks and modern handlers share native route collisions and generation
ownership. Modern requests use genuine captured Request/Response primitives, with
opaque native token strings mapped to bounded private controller IDs. No lossy u64
conversion, whole-body IPC conversion or renderer-accessible registration is used.

Native route owners retain pending body requests through EOF/cancellation. A pull
event carries real credit; the private Host accepts only bounded Uint8Array replies.
Queued requests/pulls and replies check that the actual native window owner remains
live. Retirement invalidates native and JavaScript ownership before callbacks can
reenter. Post-await checks also prevent a resolved body result being published after
unhandle, abort or shutdown. Main net.fetch uses this same registry and preserves
per-hop HTTP transport/proxy checks, redirect credential stripping and owned aborts.
Standalone Mite fetch and utility networking gain no desktop-session registry.

File interception is an explicit route kind, not an origin privilege. Native policy
generations travel in the helper startup snapshot and authenticated ordered control
messages. Active interception without an exact native document/navigation grant
fails closed. Late activation refreshes the actual canonical document ID, page host,
URL and settings binding; an empty committed navigation label grants no extra
authority. Policy delivery is ordered before subsequent main-issued navigation and
grants; already initiated renderer work is not atomically rolled back.

The normal scheme-fetch adapter intercepts before native file loading. A separate,
narrow pinned seam admits owned file-Window programmatic CORS GET/HEAD after upstream
CSP/mixed-content/local-URL checks and before the non-HTTP CORS refusal. It supplies
only the application's handler response, never a native filesystem fallback. Other
origins, workers, frames and unsupported request modes retain their restrictions.
FileModules and file-origin classification are unchanged. The main load binding
also skips its regular-file stat when an actual file response handler owns the URL.

## Evidence and limits

Public offscreen acceptance uses exact production application objects and genuine
WebContent processes. It checks 4-MiB binary responses, 404/500 and HEAD, forbidden
headers, streaming cancellation, generations, legacy file routes, file policy
removal/reactivation, own file HTML/binary responses, CSP refusal before handler
delivery, and closed-window request/reader cancellation. Native policy/Host tests
and Node/Mite differential gates separately validate bounds, malformed input,
provenance and ownership. No visible desktop interaction is needed for these gates.

The unchanged pinned Code-OSS protocol service passes actual file denial and managed
remote 404/500/binary/MIME consumers through genuine main APIs, plus IPC registration
and cleanup. Its environment/log/remote peers are controlled collaborators; this
does not run a remote service or establish a workbench. The real diagnostic main
entry advances to @vscode/policy-watcher, whose V8 stack helper/native addon require
a genuine owned native policy integration rather than empty-policy or stack stubs.

HTTP(S) interception, partitions, workers/frames, ranges/uploads and broader request
fields remain outstanding. Main bypass has an HTTP(S) transport only. Fresh native
Linux/Windows desktop and packaged acceptance remain separate requirements. See
the [protocol contract](../api/protocol.md) for exact limits and differences.
