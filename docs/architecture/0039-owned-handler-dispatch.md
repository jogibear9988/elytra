# 0039: Protect native event handler entry and return conversion

Status: internal macOS ARM64 SDK/helper slice tested. Public preload/contextBridge/
IPC remain unsupported; handler storage isolation and resource release are open.

## Decision

Guard the free native invoke_event_handler entry before source receiver/Event
wrapping. Handler replacement can reuse a listener originally created in the
principal realm, so the existing listener callback guard alone is insufficient.
Check associated/actual source realms independently and retain their owners while
calling the complete original handler implementation. Capture abrupt completions
before the event dispatcher can put raw source errors into page events.

Reuse the existing EventTarget compile adapter. Plain token renaming would rename
both the handler definition and its internal call, bypassing the guard. Instead,
a build-time entry-name generator renames only the two free function definitions
(listener and handler); internal calls continue to use the public guarded entry.
It requires exactly one definition of each and preserves every implementation
body, license notice and other byte. No generated upstream source is committed.
The generated include has an original-source #line directive and explicit build
dependencies. Only our adapter receives its include directory. Existing notification
and abort-ID expression adapters are unchanged; there is no additional TU seam.

Source handler return objects must not reach conversion after the retained frame
ends. Ordinary/error handlers consume only strict booleans, so return booleans or
undefined. For an actual BeforeUnloadEvent named beforeunload, preserve nullish
handling, set the canceled flag before conversion (even on failure), skip conversion
when returnValue is already nonempty, and otherwise convert to a primitive string
inside the guarded owner frame. Original native processing then stores the string.
Count conversion errors without inspecting payloads. Current source code may finish
after logical detach, with physical teardown deferred until conversion returns.

## Evidence

The generator regression test compares full source changes, allowing only the two
definition-name edits, requires the internal handler call to remain public, and
rejects missing/duplicate definitions. It runs with the existing callback hook gate.
Upstream token guards now require two handler-entry occurrences (definition/call).
SDK tests use native listeners installed in the page before source replacement.
They verify handler receiver/Event wrappers, strict false cancelation, ignored
object results without traps, special error-handler arguments/return, error privacy
and detached rejection before wrapping. Last-owner detach in a handler and in
beforeunload toString must permit current DOM work, finish native teardown and
restore VM context. Throwing conversion preserves canceled=true and empty returnValue;
preexisting returnValue skips hostile conversion. A native C++ fixture constructs
the otherwise nonconstructible BeforeUnloadEvent; it exposes no page capability.

All gates pass in build/handler-acceptance-test.log: SDK, sandboxed helper, ordinary
renderer/compositor/process and native Firefly/Electron reference. The helper tests
page-created native listeners replaced by source handlers, error/ignored-object/
return behavior, stable DOM after source-only detach, continued page listener/error
controls and a self-removing frame's in-flight handler. The helper's final close
also retains the detached beforeunload replacement without reaching source wrappers.

## Limits and next boundary

This is dispatch/return protection, not isolated handler storage. Source inspection
shows EventTarget's native handler map is shared, and generated callback-valued
getters return R->callback directly (for example HTMLElement's onclick getter).
That can share raw function references in both directions, bypassing guarded native
dispatch when called directly. Existing source handler values can also remain in
that map after native listener removal. World-specific storage/getter/setter
semantics and targeted handler cleanup must be implemented before public preload.
Do not infer their safety from passing dispatch tests.

Script-valued event fields, generic WebIDL object operations, other observer kinds,
direct transfers, typed results, workers/finalization, navigation/BFCache and hard
budgets remain open. No full reclamation/audit, new Windows/Linux/package or visible
support is claimed. Ladybird stays unmodified; no newer pin was tested. Existing
linker/deployment warnings and the locked-desktop visible gate remain.
