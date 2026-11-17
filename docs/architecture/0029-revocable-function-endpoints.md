# 0029: Revocable data-only function endpoints

Status: historical initial synchronous design, extended by subsequent contextBridge
work. Public preload/contextBridge and bounded IPC now have scoped acceptance;
see the [current API contract](../api/context-bridge.md).

The initial checkpoint below described frozen function results. Commit 1363fc9
corrected this against Electron: synchronous and Promise results are mutable,
independent target copies; initial API publication and copied callback inputs
remain frozen. The 2026-11-17 actual headless helper batch validates this contract,
including top-level/srcdoc copies, nested Promise result mutations and unchanged
source/fresh results. Earlier unsupported and lifetime limitations below describe
the original design rather than current support.

## Decision

FunctionBridge exports one source function from an owned isolated world into
that exact document's principal realm. Require a live world, a fully active
document, matching source function realm and matching LibWeb principal realm.
Callable proxies and bound functions have no own realm and are rejected without
running traps. Give the target NativeFunction an explicit optional realm: the
first NativeFunction::create parameter alone only selects the allocation VM.
Freeze its own properties and verify its actual realm before publication.

Accept up to 64 arguments. Capture the entire argument array together through
BridgeData to preserve cross-argument aliases; reconstruct frozen source objects,
invoke with undefined receiver, then snapshot/reconstruct the result in target.
Inherited target setters and caller-supplied receivers grant no source references.
Reject unsupported arguments before invoking the source. Functions, promises and
host objects are still unsupported as arguments/results. Source-side effects of
an accepted call are not rolled back if its result cannot be copied.

IsolatedWorld now shares its rooted execution-context boundary between evaluate
and call. Require the called function's own realm, reject recursive entry, retain
the owner while active and defer physical detach until context pop. FunctionBridge
also rejects nested entry, including reentry through a trusted native callback.

Keep explicit roots for source function and exact document plus a strong source
world owner. The native lifecycle registry MUST revoke before detaching a record:
target globals may otherwise retain these roots through their proxy closure.
Revoke clears every root/owner and is idempotent. A retained JS proxy then keeps
only a revoked state. During invocation retain local roots so a synchronous native
callback may revoke/detach safely without destroying an active frame.

Check document activity and world detach before invocation and again after source
execution. An inactive/detached endpoint is permanently revoked; revoked in-flight
calls may not publish results. The helper revokes on realm destruction, inactive
document sweep and shutdown before world detach. Each record owns one endpoint;
the existing 128-world budget bounds this prototype's live endpoint registry.

Never inspect, stringify or copy a thrown source exception. Replace all call
rejections with the fixed target-realm TypeError "Firefly bridge call rejected".
Do not attach source stacks, names, thrown objects or privileged properties.
This internal policy is not yet an Electron error-compatibility contract.

## Evidence and limitations

The SDK gate verifies copied/frozen argument and result identities, aliases across
arguments, cycles, ignored receiver, target constructor paths, input rejection
without getter/proxy traps, arity/output budgets, sanitized thrown proxies,
unsupported function/Promise results, reentry, GC, repeated revocation and
successful calls after ordinary errors. Revoke/detach inside a running source
callback denies its output, safely restores context depth and rejects subsequent
calls through the retained proxy.

The actual sandboxed helper exports only a compiled harmless arithmetic probe.
Top-level and srcdoc first scripts verify round-trip copies, realm-local
prototypes, frozen objects and inaccessible source globals. Parent scripts retain
child endpoints, remove the frame and require immediate rejection, before the
10 ms sweep runs. The unchanged helper negative control and 70-cycle teardown
gate remain. The normal Firefly app still uses the original helper.

This is synchronous endpoint revocation, not complete document-generation/async
revocation. Queued promise jobs, listeners/timers, BFCache restore, cross-process
navigation and hard CPU/memory containment remain open. A source that returns a
Promise is rejected but may already have scheduled work. No arbitrary user preload
or capability may be enabled until async lifetime, Promise/function membranes,
validated bounded IPC and sender/frame authorization are implemented and tested.
Snapshot enumeration scratch bounds remain as documented in ADR 0028. No upstream
source changes, new platform support, package or full security audit is claimed.
