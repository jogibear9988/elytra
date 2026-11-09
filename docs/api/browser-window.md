# BrowserWindow

Availability: Firefly main process; partial native macOS ARM64 implementation.
Each window owns a [webContents](web-contents.md) EventEmitter for loading/content
observations. Rendering uses Ladybird, so Chromium web behavior is not guaranteed.

## new BrowserWindow([options])

Construct after app.whenReady. Returns a live BrowserWindow with numeric id and
webContents. options defaults to an empty object; arrays/null/nonobjects reject.

| Option | Accepted value / default |
| --- | --- |
| width | Integer 1..16384, default 800; outer DIP unless useContentSize |
| height | Integer 1..16384, default 600; outer DIP unless useContentSize |
| x, y | Optional signed 32-bit integer outer-window coordinates in DIP |
| useContentSize | Boolean, default false; dimensions refer to the client area when true |
| title | String, default Firefly |
| show | Boolean, default true |
| frame | Boolean, default true; false removes native title bar and enables custom CSS drag regions |
| minWidth, minHeight | Integer 0..16384, default 0; minimum size; constructor useContentSize also applies |
| maxWidth, maxHeight | Integer 0..16384, default 0; maximum size; each zero axis is unbounded |
| backgroundColor | Opaque color string, default white; [native canvas background](window-background.md) |
| webPreferences | Object with only the restricted values below; omitted/null uses defaults |
| webPreferences.contextIsolation | true only |
| webPreferences.nodeIntegration | false only |
| webPreferences.sandbox | true by default; boolean false is overridden after early [app.enableSandbox()](app.md#appenablesandbox), otherwise explicitly rejected |
| webPreferences.additionalArguments | Bounded string array copied to isolated-preload process.argv; nonempty arguments require a preload; see [sandbox process](sandbox-process.md) |
| webPreferences.preload | Absolute path to a bounded UTF-8 CommonJS preload; see [preload/contextBridge](context-bridge.md) |

Unknown window/preferences options and unsafe preference values throw
ERR_FIREFLY_UNSUPPORTED_API. Invalid types/dimensions throw
ERR_INVALID_ARG_TYPE or ERR_INVALID_ARG_VALUE. Construction before readiness throws
ERR_FIREFLY_NOT_READY; after app exit it throws ERR_FIREFLY_APP_EXITED.

```js
import {app, BrowserWindow} from 'firefly';
app.on('window-all-closed', () => app.quit());
app.whenReady().then(async () => {
  const window = new BrowserWindow({width: 1000, height: 700, show: false});
  window.once('ready-to-show', () => window.show());
  await window.loadFile('index.html');
}).catch(error => {
  console.error(error.message);
  app.exit(1);
});
```

ready-to-show is an observed native paint event; exact Electron presentation timing
and single-emission parity are not established. Showing immediately with the default
show:true is also supported.

## Controls and queries

| Signature | Return / behavior |
| --- | --- |
| show(), hide(), focus() | undefined; native control |
| isVisible() | boolean |
| setSize(width, height[, animate = false]) | undefined; dimensions as above; truthy animate rejects as unsupported |
| minimize(), maximize(), unmaximize(), restore() | undefined; native state transitions |
| isMinimized(), isMaximized() | boolean; native state queries |
| getSize() | [width, height] from native window |
| getBounds(), getContentBounds() | Fresh native outer/client `{x, y, width, height}` DIP rectangles |
| setBounds(rect[, animate = false]) | Partial outer rectangle; preserve omitted fields |
| setContentBounds(rect[, animate = false]) | Complete client rectangle |
| getContentSize(), setContentSize(width, height[, animate = false]) | Client size excluding native frame |
| getPosition(), setPosition(x, y[, animate = false]) | Outer-window top-left DIP coordinates |
| getMinimumSize(), getMaximumSize() | Fresh [width, height] outer DIP limits; zero means no requested limit |
| setMinimumSize(width, height), setMaximumSize(width, height) | Set outer DIP limits, each integer 0..16384; returns undefined |
| center() | Use native AppKit window centering on macOS |
| setTitle(title) | undefined; title must be string |
| getTitle() | Native window title string |
| setBackgroundColor(color), getBackgroundColor() | Opaque native canvas base; getter returns uppercase #RRGGBB. [Scope](window-background.md) |
| loadFile(path[, options]), loadURL(url[, options]) | Forward to webContents; Promise<void> |
| close() | undefined; synchronous cancellable close request, native closure follows |
| destroy() | undefined; bypass close event; native closure follows |
| isDestroyed() | boolean; safe after destruction |

Most controls/queries throw ERR_FIREFLY_OBJECT_DESTROYED after destruction.
destroy is idempotent for destroyed/already-closing windows. Do not assume that
calling destroy makes isDestroyed true before the native closed notification.
A pending load rejects when the window is closed. Loading has its own
[validation and Promise contract](web-contents.md).

## Minimum and maximum size

The constructor accepts minWidth/minHeight/maxWidth/maxHeight before the native
window is shown. Initial bounds and subsequent size/bounds changes clamp to the
native limits. With useContentSize:true constructor limits describe the client
area; getters include the actual native frame. Runtime constraint setters always
use outer-window DIP, independently of useContentSize.

Zero clears that axis. If minimum exceeds a requested maximum, minimum wins;
clearing minimum restores the remembered maximum. Changing a limit does not
immediately resize the current window on the tested macOS backend. It changes
native window-manager hints and the policy for subsequent programmatic resizes.

```js
const window = new BrowserWindow({minWidth: 640, minHeight: 400});
window.setMaximumSize(1200, 0); // Width capped; height independently unbounded.
window.setBounds({width: 1600}); // Clamped to 1200.
window.setMaximumSize(0, 0); // Clear both maximum limits.
```

Firefly intentionally requires integer inputs in 0..16384. Invalid types and
nonintegers throw ERR_INVALID_ARG_TYPE; negative/out-of-range integers throw
ERR_OUT_OF_RANGE before native mutation or window creation. An undefined
constructor field uses zero. This differs from Electron's constructor coercion
and negative-setter normalization. Firefly reports an unbounded axis as zero,
including partial constructor maxima; Electron can report INT_MAX for that case.
Positive outer limits smaller than the native frame are raised to the actual
frame plus one client DIP. Getters report that effective platform floor; Electron
can retain the smaller requested value. No fixed title-bar height is assumed.

Acceptance: shared public cases pass hidden Electron 45.2.3, Firefly offscreen and
actual macOS Cocoa. An exact-application native offscreen probe additionally
checks QWindow hints and direct native geometry enforcement. Interactive drag,
mixed-DPI transitions and fresh Linux/Windows execution remain unverified.

## Outer and content geometry

Geometry methods use the same primary-top-left DIP coordinate system as
[screen](screen.md). Other displays can have negative origins. Native frame
margins come from QWindow; there is no fixed title-bar-height guess. getBounds
and getSize include decorations; getContentBounds and getContentSize exclude
them. For frame:false the two sizes match. Constructor useContentSize changes
the meaning of width/height, while x/y always position the outer frame.

This corrects the earlier adapter, which treated all sizes as Qt client sizes.
An app requesting a framed 800-by-600 outer window now receives a smaller client
height rather than an oversized outer window. Resize updates both QWidget and
its native QWindow so queries work immediately for hidden windows; native
window placement is not forced when constructor coordinates are omitted.

```js
const work = screen.getPrimaryDisplay().workArea;
const window = new BrowserWindow({
  x: work.x + 40, y: work.y + 40,
  width: 900, height: 650, show: false
});
window.setBounds({width: 1000}); // Keep the current x/y/height.
window.setContentSize(800, 500);
window.center();
```

Rectangle setters accept only own x/y/width/height fields. setBounds permits
omitted fields; setContentBounds requires all four fields. Unknown fields reject
with ERR_FIREFLY_UNSUPPORTED_API. Position fields must be signed 32-bit integers;
invalid coordinates throw ERR_INVALID_ARG_TYPE. Dimension limits remain 1..16384
with ERR_INVALID_ARG_VALUE. Endpoint overflow rejects with ERR_OUT_OF_RANGE;
native frame conversion additionally checks decorated coordinates before calling
Qt and raises RangeError if those cannot be represented. Truthy animation requests explicitly reject. All geometry methods retain
the destroyed-window guard and synchronous return behavior.

Native minimum frame/client sizes and OS placement rules may constrain results;
query the actual bounds afterward. Very small outer dimensions retain at least
one client DIP. center uses NSWindow.center, including macOS's intentionally
higher-than-midpoint vertical placement. It does not impose a mathematical
work-area midpoint. Exact Electron multi-display selection, constrained
minimum, maximized/fullscreen/restore, mixed-DPI and physical multi-display
behavior is not established. A move can generate more than one native position
notification; exact Electron event counts/timing are not promised.

The windowless [policy suite](../../firefly/tests/window/geometry-policy.mjs)
runs the real API factory in Node/Mite, with explicit synthetic frame margins,
negative origins, partial setters, content-sized/frameless construction, centering,
snapshot ownership, validation and destroyed guards. The native geometry probe
tests checked frame arithmetic without a GUI application. The shared
[desktop fixture](../../firefly/tests/migration/geometry.mjs) exercises actual
hidden native windows against Electron, separately from synthetic policy tests.

## Instance properties

### window.id

Numeric native window identifier, assigned at construction. Use it with
BrowserWindow.fromId(id). It identifies a window, not a renderer process. Treat it
as application read-only: the current JavaScript field does not enforce immutability.
Do not reuse a saved id after closure.

### window.webContents

The window's associated [webContents](web-contents.md) instance, created with the
window. Its lifetime follows its owner. Treat this field as read-only; replacing
it does not replace the native renderer. There is no constructor for public
standalone WebContents instances.

## Instance method details

Unless stated otherwise, methods take no arguments and return undefined. Calls
are synchronous requests to the native adapter; they do not return a Promise for
animation, focus acquisition or presentation. After destruction, controls and
queries except isDestroyed/destroy throw ERR_FIREFLY_OBJECT_DESTROYED. While native
closure is pending, do not continue issuing controls: adapter errors during that
interval do not have a stable public error classification.

### window.show() / window.hide() / window.isVisible()

show requests native visibility; hide requests native hiding without destroying
the window or removing it from the registry. isVisible returns a boolean from the
native window. Hidden windows still need explicit closure. Visibility does not
prove that content has painted or that another application has not covered it.

### window.focus()

Raises and requests activation/focus for the native window. Operating-system
focus policy can refuse activation. It provides neither a success boolean nor a
focus-completion event contract.

### window.setSize(width, height[, animate = false])

- width: integer, 1 through 16384.
- height: integer, 1 through 16384.
- animate: default false; a truthy value throws ERR_FIREFLY_UNSUPPORTED_API.

Invalid dimensions throw ERR_INVALID_ARG_VALUE; there is no string coercion.
Returns undefined. This resizes the native window, not a specified content-area
rectangle. Animation and device-pixel conversion parity are not established.

### window.getSize()

Returns a fresh two-element array [width, height] from the native window's current
size. Values are numbers. Native layout/window-manager constraints can affect the
observed size; use the query rather than assuming a request was applied exactly.

### window.minimize() / window.maximize() / window.unmaximize()

Request the corresponding native state. unmaximize requests the normal state.
These operations emit observed state events asynchronously; no completion Promise
is returned. Native desktop policy and presentation are platform dependent.

### window.restore()

Clears the native minimized state and shows the window. A previously maximized
window can remain maximized. To explicitly request normal size use unmaximize.

### window.isMinimized() / window.isMaximized()

Return native state booleans. They are observations, not requests. Minimized and
maximized state must not be inferred solely from isVisible or from window size.

### window.setTitle(title) / window.getTitle()

setTitle takes a string, including the empty string, and returns undefined.
Other types throw ERR_INVALID_ARG_TYPE. getTitle returns the current native
window title. Later page-title-updated notifications can overwrite a manually set
title unless the listener prevents that automatic assignment. The document title
is separately available from webContents.getTitle().

```js
window.on('page-title-updated', event => event.preventDefault());
window.setTitle('My application');
```

### window.loadFile(filePath[, options]) / window.loadURL(url[, options])

Delegate to the associated webContents, returning its Promise<void>. Arguments,
validation, pending-load replacement and errors are documented in the
[loading reference](web-contents.md#loadfilefilepath-options). These methods do not
create another window. An invalid request can throw before a Promise is returned;
wrap the call itself in try/catch, not only a later .catch callback.

### window.close()

Requests normal closure and returns undefined, including when cancelled. The
close event is synchronous; call event.preventDefault() in that listener to cancel.
Once accepted, native closure follows and eventually emits closed. Do not use the
return value to determine whether the user cancelled. A close request already in
progress is not sent twice. It does not run renderer beforeunload.

```js
let hasUnsavedChanges = true;
window.on('close', event => {
  if (hasUnsavedChanges) event.preventDefault();
});
// After saving: hasUnsavedChanges = false; window.close();
```

### window.destroy()

Requests native destruction without emitting the cancellable close event. Returns
undefined. Repeated calls during closing or after destruction do nothing. The
closed notification still records destruction; a pending load then rejects
ERR_ABORTED. isDestroyed need not become true during this call. This differs from
[app.exit](app.md#appexitexitcode), which synchronously marks registered windows
destroyed as part of forced application shutdown.

### window.isDestroyed()

Returns the recorded destruction boolean. Safe after closure. A false result
while closing is not permission to issue additional native operations.

## Window registry

| Static method | Return |
| --- | --- |
| BrowserWindow.getAllWindows() | New array of currently registered windows |
| BrowserWindow.fromId(id) | Registered window or null |
| BrowserWindow.fromWebContents(contents) | Owning registered window or null |

getAllWindows takes no arguments and returns a new array; mutating that array does
not mutate the registry. Closing windows remain registered until their native
closed notification. fromId compares the provided key without coercion, so a
numeric id and its string representation do not match. fromWebContents compares
object identity. Unknown ids/unrelated objects return null rather than constructing
a new window.

Windows leave the registry on closure. Holding a JS reference does not keep a native
closed window usable. Keep application ownership explicit and stop background work
when its window closes.

## Events

| Event | Arguments / contract |
| --- | --- |
| close | event; synchronous preventDefault cancels normal close |
| closed | No arguments; native closure recorded, registry entry removed |
| resize | No arguments; observed native resize |
| move | No arguments; queued native QWindow position notifications |
| minimize, restore, maximize, unmaximize | No arguments; observed native window-state changes |
| ready-to-show | No arguments; native paint observed |
| page-title-updated | event, string title, boolean explicitSet; preventDefault suppresses automatic window-title assignment |

Current title notifications set explicitSet to true; this is not full Electron
parity. A window title set by the app can differ from webContents.getTitle().
The close event does not implement renderer beforeunload. Event objects expose
preventDefault/defaultPrevented, not the full Electron event structure.

## Custom title bar and dragging

Use frame:false and HTML buttons for a fully custom bar. This is different from
Electron's titleBarStyle:hidden, which retains macOS traffic lights; that option
still rejects here. The current owned adapter recognizes stylesheet declarations:

```css
.toolbar { app-region: drag; -webkit-app-region: drag; user-select: none; }
.toolbar button { app-region: no-drag; -webkit-app-region: no-drag; }
```

The host intercepts left-button drag/double-click in drag rectangles and subtracts
no-drag rectangles so buttons still receive input. Double-click toggles maximize.
Regions update at 250 ms intervals; snapshots older than one second stop intercepting
input. Navigation clears them. This is an experimental macOS adapter, not full
Electron non-client-area behavior or native resize-edge parity.

Supported scope: top-document style elements and same-origin linked stylesheets,
ordinary selectors/media rules and observed rectangular layout. Inline style
attributes, shadow-root/frame styles, @import chains, cross-origin sheets, strict CSP
that blocks the alias stylesheet, live CSSOM-only edits and browser zoom scaling
are not validated/supported. At most 256 matching elements are accepted. Put the
bar in the top document, use same-origin CSS, and allow its style policy. No native
bindings are installed in page JS; the owned shim only supplies bounded geometry.

Minimize/maximize/restore states match the pinned Electron fixture on this host.
Firefly state events pass; Electron maximize-event delivery could not be established
in the locked desktop session, so full reference event timing/presentation remains
unverified. See [ADR 0055](../architecture/0055-custom-window-chrome.md).

## Examples

[Quick start](../examples/quick-start.md), [custom window chrome](../examples/window-customization.md)
and [lifecycle](../examples/lifecycle.md) have real shared native startup assertions.
The [complete desktop catalog](../examples/index.md) also supplies reference code
for native features still missing from Firefly; each page states its exact status.

## Missing surface and evidence

Menus, fullscreen, bounds/position, parent/modal windows, icons,
platform material/style options and the wider Electron window surface are absent.
Only constructor options listed above are accepted. [Request/reply IPC](ipc.md) is
implemented for the documented subset;
[preload/contextBridge](context-bridge.md) has a bounded partial implementation.
See [security](../guides/security.md).

Implementation: [api.mjs](../../firefly/core/api.mjs) and owned native adapter.
Validation: [Firefly runner](../../tooling/tests/test_firefly.py), including controls,
registry, cancellation/destruction and unsupported options against the pinned
Electron reference. Native Windows/Linux remain unverified. See the
[compatibility matrix](../compatibility/firefly-0.1.md) for measured limits.

Native HTML [file inputs](file-input.md) now have a bounded owned Qt chooser path.
See its limits, lifecycle, helper validation and missing path/provenance APIs.
