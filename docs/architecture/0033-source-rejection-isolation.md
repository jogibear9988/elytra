# 0033: Keep source rejection objects out of page events

Status: internal adapter and macOS ARM64 SDK/helper gates implemented. Public
preload/contextBridge/IPC and full callback/error isolation remain unsupported.

## Observed boundary

LibWeb's default Promise rejection tracker uses environment settings whose global
is the document's principal Window. An owned non-main wrapper world shares those
settings. Forwarding its Promise to that tracker can therefore expose both the
source Promise and its raw reason through principal unhandledrejection and
rejectionhandled events. This is an integration boundary for our privileged-world
adapter, not a bug in ordinary page rejection behavior.

A real sandboxed-helper baseline reproduced the leak before the filter was
installed: page event handlers received at least three source reasons, including
a private function reference; later handling also produced a source handled event.
The fixture simultaneously required the page's own reject/handle controls. The
source function and marker were harmless compiled probes; no privilege was exposed.
Evidence: build/error-baseline.log. The fixture-only expect=leak query records that
historical baseline; normal acceptance always expects zero source events.

## Decision

Extend WorldJobGuard with the existing host_promise_rejection_tracker seam.
Recognize a registered Promise realm or, for Reject operations, a registered source
realm on the active execution stack. A foreign Promise constructor does not make
an owned source rejection safe to publish. Do not inspect the Promise result,
coerce/stringify its reason, read getters, build page events or delegate those
operations to the principal tracker.

Remember redacted Promise identities weakly, so Handle operations remain private
even when a foreign-realm source Promise is handled later outside the source
stack. A page Promise handled by source code still delegates its original Handle
operation: source origin alone must not suppress unrelated page tracking.
Register owned realms before any bootstrap/user evaluation; this filter does not
retroactively remove events already queued before registration.

Guarded job abrupt completions also stop using LibWeb's ordinary exception reporter,
which can dispatch raw error objects. Record only native counters for source
Reject operations and abrupt jobs. These counters contain no reason, Promise,
function, source text or stack; Reject counts are not an unhandled-error count.
Handled source failures do not become page rejectionhandled events. Original
tracking and error behavior remain for jobs/Promises outside the owned boundary.

Explicit bridge-call/Promise failures still use the separate sanitized target
TypeError path, including saved-stack target settlement. This filter does not
change already approved target results or their consumers.

## Evidence and remaining work

The SDK gate probes delegation to the original tracker. Source and foreign-Promise
rejections, including a derived foreign Promise whose handler throws, never reach
it. Native handling of the foreign Promise outside the source remains redacted.
A page Promise rejected normally and handled by source code still produces the
original Reject/Handle operations. Proxy reasons have zero getter calls, and GC
plus all existing lifetime/context tests pass.

The actual helper fixture now requires zero source unhandled/handled events and
the page's own unhandledrejection/rejectionhandled events exactly once each. PID
counters prove the source Reject operations occurred. Existing ordering controls,
Promise/frame-removal/active-frame tests and 70 frame cycles remain passing.
No general abrupt-engine-job test was synthesized through private PromiseJob
constructors; that route's payload-free reporting is implemented but not separately
triggered by this fixture.

Native counters are prototype diagnostics, not a finished preload error API.
Timer/queueMicrotask/DOM/WebIDL error routes, direct and cross-world callback
transfers, finalization/workers, navigation/BFCache, validated IPC, hard resource
containment, native Windows/Linux and packaging remain open. This is not a full
security audit. The normal Firefly app still uses its original helper, public
preload remains rejected, and Ladybird stays completely unmodified.
