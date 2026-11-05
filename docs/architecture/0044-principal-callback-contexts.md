# 0044: Separate principal callbacks from ambient Source execution contexts

Status: internal macOS ARM64 SDK/helper slice tested. Public preload/contextBridge/
IPC and general callback isolation remain incomplete.

## Decision

A Source dispatch can synchronously call a principal listener. Leaving the Source
VM stack underneath that listener exposes Source frames through page Error.stack;
it also misattributes page Promise jobs and timers to Source. Reproduce and fix
this boundary before enabling public preload.

WorldJobGuard temporarily saves the VM execution-context stack when entering an
unowned listener, handler, non-Promise Report-style WebIDL callback or native event
exception reporter from an active registered Source context. Run the original
operation above a synthetic context for its callback realm, then restore the stack
in reverse scope order. Ordinary calls with no Source context delegate directly.
The VM roots saved contexts. Native Source ownership/execution scopes remain active:
page reentry can logically detach/drop the last external owner, but physical cleanup
waits for the restored Source caller to finish. Do not reset that ownership state.

Exception reporting needs its own principal scope because upstream reports abrupt
listener completions after the listener call returns. Suppress reports associated
with registered Source callbacks before raw exceptions reach page reporting; keep
ordinary principal errors and onerror behavior. A principal beforeunload callback
stored in a Source-private handler map also converts its return value inside its
principal scope, preserving cancellation-before-ToString and existing-value behavior.

Extend the existing EventTarget generated adapter to rename the exception-reporting
free definition in addition to listener and handler definitions. Internal calls still
hit public guarded entries. No new TU substitution, vendored edits, layout changes
or committed upstream algorithm copies. Exact definition counts, byte-preservation/
drift tests and the unchanged upstream-pin gate remain required. Candidate upgrades
still require semantic review; this is not a stable upstream embedding ABI.

## Evidence

The negative SDK baseline in build/principal-callback-before-test.log observed eight
Source-frame disclosures, two Source-owned timers and only one surviving page job.
The original eight-case fixture after the fix observes zero disclosures, one actual
Source-owned timer and all eight page jobs. The final SDK adds one native Report
callback probe and observes zero/one/nine; its last-owner/GC reentry case adds a tenth
surviving page job. Source jobs remain blocked and native Source timers are removed.

All four gates pass in build/principal-callback-final-test.log: SDK isolation,
sandboxed actual helper, ordinary renderer/compositor/process and Firefly/Electron
reference. Function listeners, object handleEvent getters/bodies, proxy lookups,
throwing listeners, handlers, onerror and Report callbacks preserve receiver/Event/
error/cancellation behavior without ambient Source frames or job ownership. The
foreign beforeunload test checks ToString stack privacy and a page Promise job
surviving Source detach. Source locals/DOM wrappers survive GC while a page callback
detaches and drops the last external owner, then finish and unwind correctly.
The helper repeats the eight page cases, keeps a page interval ticking after Source
detach and verifies the Source job/timer stay silent while its active caller finishes.
Source preservation, pin integrity and strict helper signature checks also pass.

## Limits

Only these guarded native entries are covered. Generic Rethrow/NotSpecified and
Promise-returning WebIDL conversions, call_user_object_operation, arbitrary borrowed
functions/getters, other native callbacks and shared registries still require their
own policies/tests. This does not establish an API allowlist, complete realm
reclamation, navigation/BFCache isolation, worker/finalization policy or hard resource
bounds. Public preload/contextBridge/IPC stay rejected. Standalone Mite is unchanged;
no newer Ladybird pin, new platform/package, full audit or visible-desktop validation.
Existing deployment/linker warnings and the locked-desktop visible gate remain.
