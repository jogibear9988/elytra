# ADR 0190: Embedded Hub authority and native capture retirement

Status: candidate macOS native/offscreen integration validated; upstream update,
native desktop packaging and fresh platform acceptance remain under review.

## Decision

Ladybird cdcb9a separates the Qt RemoteView presentation from the canonical
Hub::View. Firefly embeds the genuine Hub in the main process and shares its
existing Core/Qt event loop. This preserves synchronous main-process handlers
without creating the browser Application, startup tabs, bookmarks or history
stores. A small owned frontend supplies the actual required platform services;
unimplemented browser actions return explicit negative results.

Qt widget identity is not document authority. Resolve native view/page/frame,
preload, permission and network ownership through the real canonical Hub view.
Keep the Hub transports alive while destroying widgets, then detach the host.
Deferred initialization seals application paths and verifies the selected native
sandbox/renderer options before ready. The pre-ready sandbox latch is a launch
plan that must match the initialized SDK options, not an invented engine state.

The embedded frontend removes a browser-store observer base from RemoteView.
Every consumer must therefore compile against the same composed header. Public
include propagation alone does not establish ordering against another dependency.
An actual offscreen crash proved this: ContentView compiled an inline state setter
against the original layout while the widget used the composed layout.

Apply the header prefix to the complete link/interface/object-source consumer
closure after target declaration. Injection runs in the SDK directory, so
SOURCE_DIR cannot identify owned targets. Verify actual compiler search paths and
keep the original standalone browser layout isolated. Unknown conditional
dependencies on embedded consumers fail explicitly. Real temporary CMake fixtures
cover injection, aliases, late targets, object sources and rejected mixed graphs.

Keep all integration compositions in owned files. Fingerprints, exact reversal,
missing/duplicate seam checks and original IPC message-ID preservation remain
mandatory. Ladybird's git checkout and license notices stay unchanged.

## Explicit captures and deferred destruction

Use GC::captures with captureless native functions. Counted native objects expose
their real cell edges; borrowed objects use the revocable lifetime anchors from
ADR 0189. Revocation precedes member destruction, and callbacks reject retired
owners before dereferencing them. Test-only mutable slots trace their current
cell rather than copying an owner and invalidating release/GC assertions.

The new collector clears weak JS pointers before incrementally destroying native
capture boxes. A dead callback proxy can therefore retain a native endpoint after
the proxy itself has been collected. CallbackRegistry must not use native-owner
destruction as the only signal for releasing callback capacity. Retire and revoke
an endpoint when its weak proxy is dead and it has no pending Promise. Keep
proxy-dead pending endpoints registered so document revocation still settles them.
This preserves the original repeated 64-slot reclamation and pending-revocation
assertions without extra collections or weaker limits.

## Evidence and remaining work

The candidate builds actual Firefly/offscreen binaries. Genuine helper world and
byte-transport gates, the full native isolation suite, all ten selected public
offscreen renderer suites, and all eleven additional public offscreen integration
suites pass. The latter include pre-ready sandbox selection, native process
identity, IPC, preload metadata/getters/zoom, application paths, command-line
policy, tracing and modern protocol generations/cancellation.

Document resource tests separately reject equal-epoch replay and verify a trusted
higher-epoch refresh with real resource delivery and old-channel retirement.
ADR 0175 supersedes ADR 0123's original blanket duplicate-ID rule. No production
policy or test deadline was relaxed to make the candidate pass.

ICU resource hashes and native CJK decoding pass after independent bundle and
tar/ZIP relocation. Full standalone Release acceptance passes all 145 suites.
Extracted macOS applications, targeted Cocoa presentation and fresh hosted
platform checks remain separate gates.
The old accepted Designer package can keep running while this candidate is built.
These results do not establish a working VS Code editor or full Electron parity.
