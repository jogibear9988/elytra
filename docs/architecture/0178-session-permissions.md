# Document-bound session permissions

Code-OSS's real main-ready startup reaches CodeApplication.configureSession after
the native policy and local-stream slices. It registers both Electron permission
handlers. Registration must affect native renderer decisions: storing callbacks
without enforcing them would make the startup trace misleading.

## Decision

Keep public handler/lifecycle policy in maintained main-process ESM and implement
the native document/IPC boundary in C++. The default session owns a monotonically
increasing generation. Main registers a synchronous check callback and an
asynchronous request-enabled flag. Neither callbacks nor native registration APIs
are exposed to page or preload JavaScript.

Use two renderer seams. A check runs after secure-context and Permissions Policy
guards and before PermissionStore caches. A separate request admission path keeps
a denied check from swallowing the request handler. Do not store custom outcomes
in the opaque-origin permission cache or the origin-wide PermissionManager.
Ordinary Ladybird behavior remains when the embedding hooks are not installed.
Firefly's helper enters its own denial mode before running application content.

The private check IPC carries the actual page, document namespace/local ID,
permission name and policy generation. The authenticated native endpoint locates
its own page and requires an active hosted document belonging to an owned view.
It derives the current frame URL, origin, embedding origin and main-frame flag
from canonical native state. Page-authored URLs are never permission authority.
The native caller retains the page and callback through VM entry, denies busy or
modal reentry, then revalidates document/generation after JS jobs have drained.

Reuse the existing authenticated asynchronous permission request transport. Native
code preserves the original document, Permissions Policy, trust and notification
activation checks before invoking the owned request broker. A native token retains
that exact document, page and generation, with a deadline and bounded ownership.
Queued delivery, completion and timeout revalidate the same identity. Retire the
token before invoking a completion; stale or duplicate replies cannot grant.

Permission grants belong to individual native documents. They do not grant every
window sharing an origin. Generation changes, navigation, close and shutdown
revoke pending ownership and grants; replaced frame records are also pruned.
The actual native feature-use gate consults these records or a fresh authenticated
check. Microphone query/use grants remain denied and media callback(true) fails
explicitly. This slice does not supply a device backend or OS privacy entitlement.

## Upstream integration

Keep Ladybird's gitlink and source files unchanged. Strict, owned source generators
compose the pinned Permissions.cpp, Notification.cpp, WebContentPage.cpp and
WebContentClient.cpp seams. Notification requests use their actual decision,
independently of the separate query result. They check source digests, unique needles, reversal of unrelated bytes and
rejection of missing/duplicate seams. Compose with the existing native IPC and
navigation hooks rather than replacing those adapters.

The private native IPC generator preserves upstream endpoint IDs. Permission policy
travels in a guarded helper-launch argument and ordered native updates, separately
from preload capabilities and protocol/file grants. Existing out-of-process frame
hosts also receive policy updates. A missing, stale or unbound owned WebContent helper policy denies. Separate
out-of-process worker queries are not integrated and can retain upstream default
status; worker permission support remains required.
Do not widen LibWebCommon to depend on the UI permission broker.

## Differences and acceptance

Default/reset denial, cancellation on every handler replacement and denial while
the VM is busy are deliberately stricter than Electron. Native requests awaiting
admission use the policy current at admission. PermissionStatus change-event,
worker, persistence and full device/service compatibility remain separate work.

Acceptance uses controlled Node/Mite facade cases, genuine renderer queries and
geolocation denial, and a native-only test frontend linked with the production
application objects. The native driver sends valid, stale and forged identities to
the actual receiver using an actual owned document. It adds no production JS test
API. Offscreen surfaces can be virtually visible to satisfy geolocation's visibility
guard without presenting a desktop window. Denial tests request no OS permissions.

See [the API contract](../api/session.md) for exact limits and
[the current progress log](../PROGRESS.md) for observed results. A permission
component pass is not a successful Code-OSS workbench, notification delivery,
location result, microphone/camera grant or native Windows acceptance.
