# 0035: Release native timer resources with their world

Status: internal adapter and macOS ARM64 SDK/helper/app gates implemented. Public
preload/contextBridge/IPC and complete resource/callback coverage remain unsupported.

## Decision

Stopping callback execution is insufficient for repeating native timers. Observe
the original WindowOrWorkerGlobalScopeMixin timer map through a second owned
compile adapter. Preinclude its unmodified class declaration, then include the
complete original implementation with only m_timers access expressions adapted to
a reference view. Do not change the member's type, class layout or timer algorithm.
The view forwards reads/iteration and GC visitation to the actual map, observes
set/remove/clear, and never copies the container or its roots.

CMake requires exactly one callback TU and one timer-map TU substitution. Preserve
normal source/generated paths; pin/count checks currently require 15 timer-member
occurrences. These implementation seams require review on upstream updates, not
vendor edits or a patch stack. All unregistered timer behavior uses upstream code.

Associate new entries with the nearest registered realm on the execution stack,
the actual Window, its map address, timer ID and exact HTML Timer object. Keep only
weak GC references; native metadata must not retain the source realm/window/timer.
Interval rearming reuses the same object and preserves ownership even after its
callback frame has returned. Remove ownership records on ordinary clear/completion
and map clearing. Work without an owned source origin delegates unchanged.

Add native physical-detach hooks to IsolatedWorld. Run them before releasing its
SDK world/context roots, including destructor cleanup. Mark cleanup logically detached, guard reentry and
retain a live owner until its native hooks finish; try_ref must not resurrect an
owner already being destroyed. Hooks are registered before physical cleanup. A logically detached but
still executing frame may create a timer; collect it during the deferred physical
cleanup at frame pop. For an already invalid/missing owner, root the new timer and
Window until a cancellation microtask: upstream starts it after map insertion, so
stopping it before that start would create an active orphan.

Cancellation first removes metadata to permit native clear reentry, stops each
specific Timer and calls the actual Window's clear_timeout only if its ID still
maps to the expected object. Never clear the entire Window map or cancel an entry
that has since changed identity. Public cross-kind clearTimeout/clearInterval
semantics remain upstream. A small exported native wrapper calls non-exported
Timer stop/is_active methods from inside LibWeb without changing upstream exports.

## Evidence

The SDK creates live native intervals/timeouts, forces GC before holding a test
snapshot (proving the real map still traces timers), verifies they are armed, then
clears them through both cross-kind APIs. Require empty ownership metadata and
inactive actual native timers. Repeat, detach the world and verify two canceled
entries plus inactive rooted Timer objects. In-flight reaction/await/generator/
queueMicrotask tests create timers after logical detach and require cleanup at
physical teardown, with subsequent GC and restored owner/context invariants.
A native cleanup hook also clears the last external owner, reenters detach and
attempts evaluation: cleanup remains safe and evaluation is denied.

The actual helper starts a source interval and timeout alongside an existing page
interval in the same live document. After a source tick, detach only its isolated
world. Require stable source DOM, no timeout payload, two canceled native entries
logged against its renderer PID, and continued ticks from the existing page timer.
This cannot pass by destroying the document or clearing every timer. All existing
SDK/helper tests, ordinary renderer/compositor/process checks and native Firefly/
Electron reference fixtures pass.

## Limits

This proves function-timer ownership/cancellation, not full timer compatibility or
resource containment. String-handler realm semantics still need policy: upstream
creates those classic scripts in the global settings realm. Observer/listener
cleanup, Promise-returning/rethrowing WebIDL paths, typed callback results, direct
function transfers, worker/finalization paths, BFCache/navigation and queue/CPU/
memory budgets remain open. Source-origin association is conservative across
foreign callees. No complete realm reclamation, new native Windows/Linux support,
package or full audit is claimed. The normal app still rejects preload; Ladybird's
checkout remains unmodified. The visible-desktop gate remains separate/open.
