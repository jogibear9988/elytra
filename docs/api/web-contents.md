# webContents

Availability: the main-process object at BrowserWindow.webContents. Partial native
macOS ARM64 implementation; an EventEmitter associated with the owning window.
It observes a separate Ladybird WebContent process and has no general script/IPC API.

## loadFile(filePath[, options])

filePath is a string; relative paths resolve against the application entry module's
directory, not the caller module or current working directory. The host converts it
to a file URL and calls loadURL. Returns Promise<void> for navigation completion.

options must be absent or empty. Query/hash and other Electron loading options are
unsupported. A nonstring path throws ERR_INVALID_ARG_TYPE; nonempty options throw
ERR_FIREFLY_UNSUPPORTED_API. A missing/unloadable file rejects navigation.

## loadURL(url[, options])

url is a string parseable as an absolute URL. Only file:, http: and https: are
accepted. options must be absent or empty; headers, post data and other options
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

## Unsupported surface and evidence

executeJavaScript, send, openDevTools, session, general frame APIs and the wider
WebContents event/method surface are absent. Public preload/contextBridge/IPC are
not enabled by internal helper research. Read [security](../guides/security.md)
and [Electron migration](../migration/from-electron.md).

Implementation: [firefly.js](../../firefly/core/firefly.js).
Validation: [Firefly runner](../../tooling/test_firefly.py), file/HTTP loading,
failures, PID, lifecycle and unsupported-mode fixtures; see the
[native gates](../development/renderer-build.md) and [matrix](../compatibility/firefly-0.1.md).
