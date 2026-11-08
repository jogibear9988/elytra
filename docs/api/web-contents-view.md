# WebContentsView, contentView and webContents

Status: **partial, locally accepted functional composition**. A shared offscreen
fixture passes against real Firefly production objects and Electron 45. It exercises
real pages, renderer PIDs, viewport updates, native child order, ownership/reparenting,
detached navigation, binary IPC, cleanup and registry queries. Seven separate policy
groups pass in Node/Mite; the full non-GUI runtime gate passes all 88 suites.
The same complete fixture also passes in a hidden batch on the production Cocoa
frontend. This verifies native functional lifecycle/IPC, not on-screen macOS
composition, hardware/GPU rendering, accessibility behavior or performance. Native
Linux/XCB execution and Windows acceptance remain unverified.

```js
import { app, BrowserWindow, WebContentsView } from 'firefly';

app.whenReady().then(async () => {
  const win = new BrowserWindow({ width: 900, height: 600 });
  const child = new WebContentsView();
  win.contentView.addChildView(child);
  child.setBounds({ x: 20, y: 20, width: 400, height: 300 });
  await child.webContents.loadFile('./child.html');
  win.on('closed', () => child.webContents?.close());
});
```

Electron compatibility exposes the same subset. This is an independent native child
view with its own contents/document/channel identity, not an iframe or hidden
BrowserWindow. BaseWindow and View constructors are not exported. The window's
internal main web view remains a separate sibling, outside contentView.children.

## Construction and geometry

new WebContentsView([options]) requires app ready. Only webPreferences is accepted,
using the existing supported BrowserWindow preference subset, including documented
preload/additionalArguments behavior. Adopting existing webContents, custom sessions,
subframe preloads and unsupported preferences fail explicitly. Subclassing is
unavailable. The read-only webContents reference becomes undefined after its native
contents are destroyed; a previously saved contents wrapper remains inspectable
through isDestroyed().

| Member | Current behavior |
| --- | --- |
| setBounds({x, y, width, height}) | Requires all four own finite numeric data properties. Rounds with Math.round; negative dimensions become zero. Dimensions are bounded to 0..16384, coordinates and rectangle edges to signed 32-bit native limits. Unknown properties/accessors and animation options are refused. |
| getBounds() | Fresh logical rectangle relative to the parent. Initial rectangle is zero-sized. |
| setVisible(boolean), getVisible() | Desired visibility, initially true; detached contents can remain logically visible without appearing in a window. |
| setBackgroundColor(), setBorderRadius() | Explicitly unsupported. |
| Nested child views | Unsupported; a WebContentsView's children snapshot is empty. |

Native geometry uses the upstream viewport push so hidden-widget geometry reaches
the actual page. Zero-area views use safe physical parking where required by Qt;
logical zero dimensions/edge coordinates are preserved. Logical state does not
promise a visible pixel or focus on a particular platform compositor.

## Attach, reorder and detach

BrowserWindow.contentView is an internal EventEmitter root supporting
addChildView(view[, index]), removeChildView(view), and read-only children snapshots.
Snapshots are independent arrays of public child views. The internal main view is
not included. Root geometry/visibility/style operations are explicitly unavailable.

Adding requires an owned live WebContentsView and live parent. The optional index
must be a nonnegative integer and is clamped to the child-list length. Re-adding
without an index moves a child to the end/top; adding to a different window reparents
it. Native attachment must succeed before JavaScript ownership/list metadata changes.
A native refusal preserves the old owner/order. Removing a valid view belonging to
another parent is a no-op. Forged/stale wrappers fail explicitly.

Moving/detaching preserves the contents ID, native view, document, renderer process
and channel identity. Navigation while detached remains possible. Closing an old
host after reparenting does not destroy the transferred child. Closing a parent
destroys its primary contents, while independent children survive detached and must
be closed explicitly when no longer needed. No GC timing or automatic collection of
unreachable native views is promised: strong native/JS registries are bounded to
64 child views and 256 total contents records. Call child.webContents.close().

## Registry, focus and close

| Member | Current behavior |
| --- | --- |
| webContents.fromId(id) | Live registered main/child contents or undefined. |
| webContents.getAllWebContents() | Fresh array of live registered contents. |
| webContents.getFocusedWebContents() | Contents reported focused by the native host, or null. |
| contents.id | Read-only permanent contents identifier. |
| contents.getOwnerBrowserWindow() | Current live attached BrowserWindow or null; BrowserWindow.fromWebContents follows the same owner. |
| contents.focus(), contents.isFocused() | Native focus request/query. Offscreen tests do not establish physical keyboard/platform focus behavior. |
| contents.close([options]) | Requests native destruction; repeated close on closing/destroyed contents is a no-op. Only an empty options object or waitForBeforeUnload:false is supported. |
| contents.isDestroyed() | Actual host lifecycle state. The destroyed event occurs once after registry/ownership cleanup. |

An explicitly detached view reports no owner. Electron can retain its last owner
in some detached cases; this is a deliberate compatibility difference. Destroyed
views leave the registry, clear ownership and retire pending IPC/navigation work.
Unimplemented beforeunload coordination is not silently simulated.

## Validation and remaining scope

The offscreen frontend uses the exact production view/bridge objects with a
test-only static platform plugin. The plugin is not shipped and does not enable a
production fallback from protected desktop platform selection. It covers CPU
functional composition and genuine
LibWeb pages/processes/IPC. Plugin warnings about setParent/raise do not establish
on-screen rendering. The Cocoa accessibility build adaptation belongs to owned build
integration, is guarded for non-Cocoa builds and preserves pinned source/license
notices. Ladybird/vendor code remains unmodified.

The actual prepared Code-OSS local desktop/NLS entry now reaches app ready and
passes WebContentsView/webContents imports on these production objects, then fails
at unimplemented app.getGPUFeatureStatus. Metadata-only desktop source support is now implemented with separately documented
acceptance limits. Notification, power and shell component gates now pass; real
notification delivery and visible shell actions remain unaccepted. Continue the
actual graph with native file polling. Import progress is not a VS Code
window or integrated browser acceptance.

No custom sessions, subframe preload integration, popups, background/radius styling,
nested views, adoption, CDP/debugger or integrated Code-OSS browser acceptance is
claimed. Actual Code-OSS BrowserView requires unsupported session/subframe preferences
and further services beyond this slice. Native utility/renderer MessagePort transfer,
authentication forwarding and a working Code-OSS editor remain separate work.
See [ADR 0157](../architecture/0157-native-content-view-ownership.md),
[BrowserWindow](browser-window.md), [IPC limits](ipc.md) and
[Code-OSS status](../compatibility/vscode.md).
Reference contracts: [WebContentsView](https://www.electronjs.org/docs/latest/api/web-contents-view),
[View](https://www.electronjs.org/docs/latest/api/view) and
[BaseWindow lifetime](https://www.electronjs.org/docs/latest/api/base-window#resource-management).
