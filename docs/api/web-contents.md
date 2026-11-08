# webContents

Availability: the main-process object at BrowserWindow.webContents. Partial native
macOS ARM64 implementation; an EventEmitter associated with the owning window.
It observes a separate Ladybird WebContent process. General script execution
remains unavailable; [IPC](ipc.md) provides preload
request/reply and bounded bidirectional events.

## Member index

| Member | Return | Role |
| --- | --- | --- |
| [loadFile(filePath[, options])](#loadfilefilepath-options) | Promise<void> | Load a local document |
| [loadURL(url[, options])](#loadurlurl-options) | Promise<void> | Load an absolute supported URL |
| [getURL()](#contentsgeturl) | string | Observe the current URL |
| [getTitle()](#contentsgettitle) | string | Observe the document title |
| [getOSProcessId()](#contentsgetosprocessid) | number | Query the native renderer PID |
| [send(channel, ...args)](ipc.md#webcontentssendchannel-args) | undefined | Native current-document event to isolated preload |
| [isDestroyed()](#contentsisdestroyed) | boolean | Observe owner destruction |

Access through window.webContents; it is not exported as a standalone constructor.
Event subscriptions use the supported EventEmitter subset. There is no general
remote execution method.

## loadFile(filePath[, options])

filePath is a string; relative paths resolve against the application entry module's
directory, not the caller module or current working directory. The host converts it
to a file URL and calls loadURL. Returns Promise<void> for navigation completion.

options must be absent or empty. Query/hash and other Electron loading options are
unsupported. A nonstring path throws ERR_INVALID_ARG_TYPE; nonempty options throw
ERR_FIREFLY_UNSUPPORTED_API. A missing/unloadable file rejects navigation.

## loadURL(url[, options])

url is a string parseable as an absolute URL. Accept file:, http:, https: and
custom schemes with an active default-session [file handler](protocol.md). options must be absent or empty; headers, post data and other options
are unsupported. Returns Promise<void>, resolved after did-finish-load is emitted.
The current loading implementation is not full Electron navigation parity.

| Failure | Current result |
| --- | --- |
| Destroyed owner | Synchronous ERR_FIREFLY_OBJECT_DESTROYED |
| Nonstring URL | Synchronous ERR_INVALID_ARG_TYPE |
| Nonempty options | Synchronous ERR_FIREFLY_UNSUPPORTED_API |
| Invalid URL | Rejected Promise, ERR_INVALID_URL |
| Unsupported scheme | Rejected Promise, ERR_FIREFLY_UNSUPPORTED_API |
| Another accepted load replaces pending load | Previous Promise rejects ERR_ABORTED |
| Window closes / app exits | Pending Promise rejects ERR_ABORTED |
| 30-second loading deadline elapses | Pending Promise rejects ERR_TIMED_OUT |
| Native load failure or renderer crash | Pending Promise rejects ERR_FAILED |

Validation happens before replacing a pending load: an invalid URL or unsupported
scheme does not itself cancel a previously accepted navigation. Each accepted load
has one pending Promise; another accepted request supersedes that Promise. There
is no public navigation identifier or cancellation token. Overlapping native
notifications are not a complete Electron navigation-history implementation.

Do not pass null as loading options: it is not a supported options object and can
throw an ordinary TypeError during validation. A rejected navigation does not
create a fallback renderer. Redirect/fragment/overlap races and detailed network
errors remain under development.

```js
// window is an existing BrowserWindow created after app readiness.
try {
  await window.loadFile('index.html');
  console.log(window.webContents.getURL());
} catch (error) {
  console.error(error.code || error.message);
}
```

BrowserWindow.loadFile/loadURL forward to these methods and share the same pending
navigation. Network rendering does not imply that Mite has an HTTP client or TLS API.

## Queries and lifetime

| Signature | Return |
| --- | --- |
| getURL() | Observed navigation URL string; initially empty |
| getTitle() | Observed document title string; initially empty |
| getOSProcessId() | Native WebContent renderer PID |
| isDestroyed() | boolean from owning window lifetime |

Queries other than isDestroyed throw ERR_FIREFLY_OBJECT_DESTROYED after the owner
is destroyed. getTitle is the page title, distinct from the window's native title.
Renderer crash is reported, not automatically recovered by this API.

### contents.getURL()

No parameters. Returns the last observed navigation URL string, initially ''. The
value can change as native URL notifications arrive. It is not a Promise, a
navigation history list or proof that a navigation completed successfully.

### contents.getTitle()

No parameters. Returns the observed document title string, initially ''. This
tracks the page even when a page-title-updated listener prevents native window-title
assignment. Use window.getTitle() for the native window caption.

### contents.getOSProcessId()

No parameters. Returns the native Ladybird WebContent process identifier as a
number. This is a separate process from the Mite main process. Do not assume a
stable PID across crashes or derive frame identities from it. Renderer restart
and process reuse policies are not a public contract.

### contents.isDestroyed()

No parameters. Returns the owning window's recorded destruction boolean; safe after
closure. There is no separate public contents.destroy method. Stop timers and
release application references when the owner emits closed.

## Events

| Event | Arguments / current behavior |
| --- | --- |
| did-start-loading | No arguments; native loading begins |
| did-finish-load | No arguments; observed completion, then pending Promise resolves |
| did-fail-load | event, errorCode, description, validatedURL, isMainFrame |
| render-process-gone | event, details |

Failure events currently use errorCode -2, native description, the last observed
URL and isMainFrame true. Crash details currently contain reason:'crashed' and
exitCode:-1. These are simplified diagnostics, not Electron's full network/crash
classification. Timeout/cancellation rejection does not guarantee a did-fail-load
event. Event objects expose preventDefault/defaultPrevented; these notifications
do not use cancellation to undo a completed navigation or crash.

### Event: did-start-loading

No arguments. Emitted from a native load-start notification. It does not expose
request headers, a navigation id or a cancellable event. Native timing relative
to page scripts is not guaranteed to match Chromium.

### Event: did-finish-load

No arguments. The URL observation is updated, the pending timer cleared and the
pending load detached before emission. Synchronous listeners run before the
corresponding Promise continuation. It reports native completion, not that every
application worker, fetch or UI task has finished. Use an application-specific
ready signal for that, as the [example smoke tests](../examples/index.md) do.

### Event: did-fail-load

Arguments: event object, numeric errorCode (-2), string description, string
validatedURL (last observed URL), boolean isMainFrame (currently true). The pending
Promise is rejected before this notification is emitted. Because validatedURL is
an observation, it need not identify the failed requested URL. Do not classify
network failures solely from the fixed numeric code.

### Event: render-process-gone

Arguments: event object and details object {reason: 'crashed', exitCode: -1}.
Rejects a pending load and reports native renderer loss. The simplified details
do not distinguish Chromium-style crash categories. This event does not recreate
the window, reload the document or restart the renderer automatically.

### EventEmitter subscriptions

on/once/removeListener and the other supported [emitter methods](mite.md) operate
locally. Registering a name such as ipc-message or dom-ready does not implement
that native event. No page-title-updated event is emitted on this object; subscribe
to the owning [BrowserWindow](browser-window.md#events).

## Renderer modules

`loadFile` pages can load external browser ESM using relative script URLs and
static/dynamic imports. See the [renderer module guide](../guides/renderer-modules.md)
for validated behavior, CSP/MIME checks and the retained file/network boundaries.
These modules have no Mite/Node/native imports.

## Examples

[Quick start](../examples/quick-start.md) and [iframe messaging](../examples/web-embeds.md)
exercise actual loaded pages. [Navigation history](../examples/navigation-history.md),
[native drag](../examples/native-file-drag-drop.md) and [offscreen rendering](../examples/offscreen-rendering.md)
provide reference implementations but explicitly remain blocked in Firefly.

## Unsupported surface and evidence

executeJavaScript, openDevTools, broader session methods, general frame APIs and the
wider WebContents event/method surface are absent. The public isolated
[preload/contextBridge subset](context-bridge.md) is implemented separately;
[ipcMain/ipcRenderer](ipc.md) supports bounded invoke/handle request/reply and
plain-data events in both directions. Binary/port IPC and full structured clone remain open. Read [security](../guides/security.md)
and [Electron migration](../migration/from-electron.md).

Implementation: [api.mjs](../../firefly/core/api.mjs).
Validation: [Firefly runner](../../tooling/tests/test_firefly.py), file/HTTP loading,
failures, PID, lifecycle and unsupported-mode fixtures; see the
[native gates](../development/renderer-build.md) and [matrix](../compatibility/firefly-0.1.md).
