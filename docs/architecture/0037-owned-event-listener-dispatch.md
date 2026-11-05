# 0037: Guard event listeners before native wrapper construction

Status: internal macOS ARM64 SDK/helper implementation. Public preload,
contextBridge and IPC remain unsupported; native listener resource release is open.

## Decision

Gate Web::Bindings::invoke_event_listener at its native entry. A fourth owned
LibWeb compile adapter preincludes the original BindingsGlue declarations, then
includes the complete EventTarget.cpp with only this free function's name adapted.
Restore the original public function as a dispatcher through the existing per-VM
callback registry. Require exactly one TU substitution and one entry token.
No class layout, generated binding, vendored source or dispatcher algorithm changes.
Unregistered callbacks delegate to the complete upstream function.

A gate only around call_user_object_operation is too late: EventTarget constructs
currentTarget/Event wrappers first. SDK tests with a detached source listener
reproduced WrapperWorld::set_wrapper's !is_detached assertion on that earlier path.
Guarding before wrapper creation avoids both detached-world entry and assertion.
The generic WebIDL operation entry remains unchanged and is not claimed protected.

Use upstream CallbackType::associated_realm through a small native exported wrapper
inside LibWeb, preserving its origin-domain rule for non-function objects. For
functions also inspect get_function_realm without executing callback getters.
Reject detached/inactive owned realms before calling upstream. Retain both owned
realms through original wrapper construction, function execution and handleEvent
lookup/call, including getters/proxy traps. IsolatedWorld's existing async-frame
mechanism defers physical detach and retains the last owner until return.

Capture abrupt completions before EventDispatcher can report the raw source
exception to the page. Count failures without inspecting the value and return
undefined to this void listener call. Normal values/receivers and preventDefault
behavior continue through upstream dispatch. Page callbacks/errors remain delegated.
No new JavaScript capability or public preload is enabled.

## Evidence

SDK controls check function/object receivers, handleEvent lookup count, source
Event/currentTarget/target wrappers despite poisoned page Event prototypes,
cancelable dispatch, sanitized function/getter failures, continued page delivery
and blocked detached callbacks/getters. GC precedes delivery. In-flight function,
getter and proxy cases detach/drop the last external owner, continue DOM work,
create a timer after logical detach and queue a denied continuation; physical
teardown, timer cleanup and VM context restoration occur after return.

The sandboxed helper repeats active/detached delivery with page error/observer
controls and matching-PID blocked/error counts. Child frames remove themselves
inside function, getter and proxy listeners and finish their current DOM work while
later microtasks are denied. Existing SDK/helper, ordinary renderer/compositor/
process and Firefly/Electron reference regression gates are required as well.

## Limits

Callback suppression does not remove native listener registrations or signal abort
algorithms. Targeted removal is the next resource slice. Event handler attributes
use a different entry and remain open. CustomEvent.detail, ErrorEvent.error,
MessageEvent data and other script-valued event fields need an explicit transfer
policy; ordinary native Event wrappers do not prove an argument membrane. Generic
WebIDL object operations, typed return conversion, direct cross-world transfers,
other observers, workers/finalization, navigation/BFCache and hard resource bounds
remain incomplete. No complete realm reclamation, Windows/Linux/package support
or full security audit is claimed. Ladybird stays unmodified; no newer pin was
tried. Existing linker/deployment warnings and the visible-desktop gate remain.
