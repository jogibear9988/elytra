# ADR 0157: Native content-view ownership independent of windows

Status: partial, locally accepted by shared offscreen Firefly/Electron functional
fixture, hidden production Cocoa batch and Node/Mite policy gates.

## Context

The actual pinned Code-OSS browser subsystem statically imports WebContentsView and
webContents. Its optional browser panels construct independent pages, attach/reparent
them through BrowserWindow.contentView, resize them, and explicitly close their
contents. The main workbench still uses BrowserWindow. Import success, an iframe,
or a hidden BrowserWindow with a projected rectangle would not implement this
native composition/lifetime contract.

## Decision

Create a genuine native child web view with a permanent contents ID and its own
renderer/document/channel identity. Keep these identities stable across attach,
reorder, detach and cross-window reparenting. Reuse the actual native bridge and
WebContents wrapper for page loading, bounded binary IPC and lifetime cancellation.
Do not create a hidden top-level window for each child. Window primary contents
remain an internal sibling and are excluded from the public child snapshot.

Maintain private host authority and bounded strong registries. Commit JavaScript
parent/order metadata only after native mutation succeeds; failed attach/detach
preserves prior ownership. Reject forged/stale objects, closing parents/children,
invalid indices, unsupported preferences and unimplemented constructor features.
Recheck readiness/shutdown after preference/emitter hooks before native allocation.
The internal root supports flat composition only; no public View/BaseWindow export,
nested hierarchy or adopted contents is supplied by this slice.

Use strict own finite bounds, rounded coordinates/dimensions, negative dimension
clamping and native 0..16384 dimension/int32 edge checks. Preserve zero-area logical
geometry while safely parking physical widgets at extreme edges. Explicitly push
viewport size through the existing upstream API because hidden Qt widgets may defer
resize events. Desired visibility is distinct from attachment or actual on-screen
visibility. Styling, animation and CDP/popup/session features remain unsupported.

Closing a parent destroys its primary contents and detaches surviving independent
children. A former parent's late closure must not steal/destroy a reparented view.
Explicit contents close requests retire native resources; final destruction first
clears registry/attachment state and pending work, then emits destroyed once. Child
webContents getter becomes undefined after destruction. Shutdown clears owned
JavaScript records without invoking native operations after native teardown begins.
Strong registries require explicit close and enforce 64 child / 256 total contents
bounds; no GC timing claim is made.

Expose real main/child webContents lookup, enumeration, focused lookup, read-only IDs,
native focus/query, explicit close and current owner lookup. Detached owner is null,
even where Electron retains a historical owner. Do not turn stale historical
association into current ownership or claim full Electron close/beforeunload parity.

## Evidence and remaining work

A shared offscreen fixture passes against actual Firefly production objects and
Electron 45. It checks real page content/PIDs, actual viewport size updates, native
QObject child order and owner queries, reparent/detached navigation, survival after
old-host closure, binary IPC cleanup and contents registry behavior. A separate
policy projection passes seven Node/Mite groups for validation, atomic failure,
identity/snapshots, exact-once destruction and shutdown reentry. The complete
non-GUI runtime gate passes all 88 suites; these policy checks are not widget tests.

The offscreen frontend uses a test-only static platform plugin around the exact
production objects. This plugin is test-only, not shipped and not a production
desktop fallback. setParent/raise warnings and functional CPU results do not
verify Cocoa on-screen composition, GPU/hardware rendering, focus or accessibility.
An owned Cocoa accessibility build adaptation is guarded for non-Cocoa builds and
preserves upstream pins/notices; no Ladybird/vendor patch is introduced. Production
Cocoa hidden functional acceptance passes the same complete fixture, including
lifecycle and IPC. On-screen composition/hardware/GPU/AX/performance remain
unaccepted; native Linux/XCB and Windows execution also remain unverified.

The actual prepared local desktop/NLS entry reaches app ready and passes
WebContentsView/webContents imports, then next fails linking createWriteStream from
node:fs. Implement genuine bounded writable file streams, then continue that graph;
this import advance does not accept a VS Code window.

Code-OSS integrated BrowserView immediately demands unsupported session and subframe
preload preferences, then additional debugger/CDP/session/window services. This
slice is not acceptance of that subsystem or a workbench. Retain explicit refusals
for adoption, nested views, BaseWindow/View exports, styling, custom sessions,
subframe preloads and popups. Continue genuine utility/renderer port handoff,
authentication forwarding and actual main/workbench workflows. Mite remains
independent of Firefly/LibWeb. See [API scope](../api/web-contents-view.md) and
[current Code-OSS status](../compatibility/vscode.md).
