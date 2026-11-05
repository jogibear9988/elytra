# 0030: Observe and revoke Promise results

Status: internal adapter and macOS ARM64 SDK/helper gates implemented. Public
preload/contextBridge/IPC and full source-job cancellation remain unsupported.

## Decision

Extend FunctionBridge results to recognize an actual engine Promise belonging to
the exact source realm, with no host class. Attach native fulfilled/rejected
reactions using perform_then, without accessing .then, constructor or species.
Do not assimilate arbitrary source thenables. Promise arguments, functions as
data and nested Promise values remain unsupported by BridgeData.

Allocate a separate target Promise using its intrinsic constructor. Fulfilled
source values pass through the same bounded data snapshot and frozen target
reconstruction as synchronous results. Reject unsupported/over-budget fulfillment
values with a fixed target TypeError. Before resolving, inspect the copied root's
ordinary prototype chain (up to 64 entries) for then accessors/callables or exotic
objects. Reject these without running target code: otherwise a poisoned target
prototype could hijack resolution into an untracked pending thenable. Source rejections never enter target code
as raw values and are never inspected, stringified or copied.

Own at most 64 pending capabilities per endpoint, indexed by a monotonically
increasing identifier. Reject calls before invoking the source when that budget
is full, including otherwise synchronous calls. Remove entries before settlement
and reject identifier exhaustion. Reaction closures capture only endpoint/id;
they do nothing when their entry no longer exists. Source promises are rooted
while observers are installed; pending target capabilities/realms have explicit
roots until settlement or revocation.

Revoke marks the endpoint invalid, clears its source function/document/world
ownership, removes every pending entry and rejects each target Promise. Late
source reactions cannot copy data, recreate entries or settle it again. Validate
document activity and source world before copying a fulfillment and again before
publication. These checks extend the synchronous endpoint lifetime policy.

The native lifecycle sweep can run without any JS execution context. Supply a
temporary native-call context for built-in reject functions when needed, without
performing a microtask checkpoint inside revoke. The real helper gate exposed a
native-function panic before this fix; do not replace it with sandbox changes.

Create bridge errors under a separately saved execution-context stack and a
synthetic target frame, restoring the caller immediately. This retains real target
TypeError identity while avoiding source traceback capture, including revoke
inside an active source call. The SDK save/restore API keeps suspended frames
rooted. The same error creation policy now covers synchronous call failures.

## Evidence and remaining boundaries

SDK tests exercise actual LibWeb microtask checkpoints with the harness's initial
context saved/restored. Require asynchronous ordering, target Promise/prototype
identity, copied/frozen fulfillment graphs, sanitized rejections and unsupported
values, zero source .then/constructor/exception getter traps, all 64 pending slots,
pre-source budget rejection, trap-free target prototype-pollution rejection,
GC, repeated revoke, 64 target rejections and ignored
late source fulfillment. Revoke/detach inside a source callback rejects an existing
pending target Promise and denies the current call without source stack frames or
context leaks.

The actual sandboxed helper adds only compiled harmless Promise probes. The first
author script verifies fulfillment copies and sanitized rejection. A live parent
handler observes a retained child Promise's rejection after frame removal. The
unchanged-helper negative control, synchronous calls and 70-cycle cleanup gate
remain passing. The normal Firefly app continues to use its original helper.

This revokes forwarding, not source execution. Existing queued source Promise jobs,
timers, listeners and other retained closures can still run after world detach.
Native reactions cannot be unregistered through the inspected SDK; source promises
and queued reactions may retain VM realms until settlement/collection. Clearing
our pending roots does not prove total realm reclamation. BFCache, cross-process
navigation, callback/function arguments, hard CPU/memory containment, validated IPC
and sender/frame authorization remain required before privileged user preload.
No public Electron Promise/error parity, Windows/Linux support, package or sandbox
audit is claimed. Ladybird source remains unmodified.
