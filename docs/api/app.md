# app

Control the Firefly application's readiness and lifetime.

**Process:** main. **Availability:** partial, tested on native macOS ARM64.
The singleton is an EventEmitter. Standalone Mite and document/preload code do not
export app. Native Windows/Linux desktop integration remains unfinished.

```js
import {app, BrowserWindow} from 'firefly';
import {fileURLToPath} from 'node:url';

app.on('window-all-closed', () => app.quit());
app.whenReady().then(async () => {
  const window = new BrowserWindow({width: 1000, height: 700});
  await window.loadFile(fileURLToPath(new URL('./index.html', import.meta.url)));
}).catch(error => {
  console.error(error.message);
  app.exit(1);
});
```

CommonJS remains supported: require('firefly').app refers to the same singleton as
the initial ESM export. The electron module name is not an automatic alias. Use the
[ESM lifecycle example](../../examples/electron/lifecycle/main.mjs) for a complete
window/load/quit trace and the [example guide](../examples/lifecycle.md) to run it.

## Member index

| Member | Return | Status |
| --- | --- | --- |
| [isReady()](#appisready) | boolean | Implemented readiness observation |
| [whenReady()](#appwhenready) | Promise<void> | Implemented shared readiness promise |
| [quit()](#appquit) | undefined | Partial cancellable application shutdown |
| [exit([exitCode])](#appexitexitcode) | undefined | Partial forced shutdown |
| [EventEmitter methods](#eventemitter-methods) | Depends on method | Supported emitter subset |
| [Other Electron app members](#unsupported-electron-app-surface) | Unavailable | Not native implementations |

## Readiness

### app.isReady()

**Parameters:** none. **Returns:** boolean, synchronously.

Initially false. The native ready notification sets it to true **before** ready
listeners execute. It remains true once initialization has been observed; closing
windows or requesting exit does not reset it. It says nothing about whether a
particular page has loaded, painted or crashed. Use webContents load promises or
window events for those observations.

Calling it before readiness is supported and does not create a window. It has no
readiness error code and does not request initialization. Extra JavaScript arguments
are ignored by the current implementation.

```js
import {app} from 'firefly';
console.log('Initialization observed:', app.isReady());
app.whenReady().then(() => console.log('Initialization observed:', app.isReady()));
```

### app.whenReady()

**Parameters:** none. **Returns:** the same Promise<void> on each call.

The Promise resolves with undefined after all synchronous ready listeners return.
If readiness has already occurred, attaching a continuation still uses normal
Promise scheduling. There is no launchInfo or Event argument in the resolution.
The Promise does not wait for a window, page load or native paint and does not
create any of them.

Register ordinary window creation with whenReady().then(async () => ...). This
lets the ESM entry finish initialization before application readiness; our shared
Electron/Firefly examples use that pattern. An early forced exit can terminate the
application before readiness is delivered; this is not a shutdown-completion promise.

```js
import {app} from 'firefly';
const first = app.whenReady();
console.log(first === app.whenReady()); // true in Firefly
first.then(value => console.log(value)); // undefined
```

Creating a BrowserWindow before readiness throws ERR_FIREFLY_NOT_READY. Creating
one after application exit is requested throws ERR_FIREFLY_APP_EXITED. Those errors
belong to BrowserWindow; whenReady itself has no corresponding argument validation.

## app.quit()

**Parameters:** none. **Returns:** undefined immediately; it is not awaitable.

Requests normal shutdown. Native window deletion and its acknowledgements can
follow asynchronously. The implemented sequence is:

1. Mark the quit attempt active and emit before-quit(event).
2. If cancelled, restore the running state and leave windows alone.
3. Request close on registered windows in order. Each close listener can cancel.
4. Once the registry is empty, emit will-quit(event).
5. If that is cancelled, keep the application alive, even with no windows.
6. Otherwise emit quit(event, 0) and request native termination.

A later window cancelling close does not reopen earlier windows already closed or
closing. A later quit call starts a new attempt after cancellation. Calls nested
inside an active attempt coalesce; they do not recursively emit another before-quit.
A call after forced/normal exit has been requested does nothing.

### Cancel and retry

preventDefault must run synchronously in the event listener. EventEmitter does not
await an async listener, and the return value false is not cancellation.

```js
import {app} from 'firefly';
let cleanupStarted = false;
app.on('before-quit', event => {
  if (cleanupStarted) return;
  event.preventDefault();
  cleanupStarted = true;
  Promise.resolve().then(() => {
    // Finish your application's bounded cleanup here.
    app.quit();
  });
});
```

Install a real cleanup/retry policy, including rejection handling, when replacing
this small example. Creating a replacement window during will-quit requires
preventDefault to cancel exit; window creation alone is not an exit veto.

### Last-window policy

Firefly currently emits window-all-closed when the last window closes outside an
active quit. Its core does not automatically quit merely because no listener was
registered. Applications choose a policy explicitly:

```js
app.on('window-all-closed', () => app.quit());
```

This differs from Electron's default unobserved last-window behavior. OS activation,
dock reopen, Cmd-Q integration, renderer beforeunload, OS logout/shutdown and
updater-initiated exit are not full Electron implementations. Do not keep a macOS
app alive with no windows expecting an unimplemented activate event to reopen it.

## app.exit([exitCode])

**Returns:** undefined. Request forced shutdown with a process status.

| Parameter | Type | Default | Accepted values |
| --- | --- | --- | --- |
| exitCode | number | 0, including explicit undefined | Integer in -2147483648..2147483647 |

| Input | Result |
| --- | --- |
| Omitted / undefined / 0 | Request status 0 |
| 7 | Request status 7 |
| String, boolean, null, bigint, NaN, Infinity or fractional number | Throw ERR_INVALID_ARG_TYPE |
| Integer outside signed 32-bit range | Throw ERR_OUT_OF_RANGE |

Validation runs before the already-exited guard. A repeated valid call after exit
is requested does nothing; it does not replace the first requested exit status.
A repeated invalid call can still throw its validation error.

Forced exit bypasses before-quit, will-quit and cancellable window close. It destroys
live windows, marks/removes their main-process objects, rejects pending navigation
with ERR_ABORTED, emits each closed and then quit(event, exitCode), and requests
native termination. window-all-closed is suppressed. The current JavaScript turn
can finish before the native process stops; future timers/Promise work must not be
used as a forced-exit cleanup contract.

```js
import {app} from 'firefly';
app.whenReady().then(() => {
  app.exit(7);
  console.log('The current turn can finish.');
});
```

An exception from a closed/quit listener still reaches the finally block requesting
native exit. Listener errors are not a cancellation mechanism. Run necessary
asynchronous cleanup before exit, or use cancellable normal quit.

The quit event receives the requested numeric code. The OS process representation
can differ: the macOS reference gate checks the conventional low eight bits for
POSIX process exit. Native Windows Firefly status delivery is unverified; no
cross-platform normalization of every signed status is promised.

## Events and event objects

### Event: 'ready'

**Arguments:** none in Firefly. **Cancellation:** no.

Emitted once by the native readiness notification, after isReady becomes true and
before whenReady resolves. Register before readiness to observe the event, or use
whenReady for code that can attach later. Firefly does not provide Electron's
ready Event/launchInfo arguments or native notification launch metadata.

### Event: 'window-all-closed'

**Arguments:** none. **Cancellation:** no.

Emitted after the last registered window closes outside an active quit. Suppressed
during normal quit and forced exit. Registering a handler does not itself request
exit: call quit if that is the desired application policy. A live background HTTP
listener or other resource should be closed as part of the chosen shutdown flow.

### Event: 'before-quit'

**Arguments:** event. **Cancellation:** synchronous event.preventDefault().

Emitted before requesting window closure on a normal quit attempt. Cancelling stops
that attempt and allows a later retry. Reentrant quit requests during this attempt
coalesce. Forced exit bypasses this event. No guarantee is made for native OS
logout/shutdown paths, which have not been integrated.

### Event: 'will-quit'

**Arguments:** event. **Cancellation:** synchronous event.preventDefault().

Emitted once the normal quit attempt has no registered windows left, just before
termination. Cancellation can leave a running app with no windows; arrange an
explicit continuation/retry. Forced exit bypasses it.

### Event: 'quit'

**Arguments:** event, exitCode (number). **Cancellation:** no.

Emitted when termination is requested: 0 for normal quit or the supplied code for
forced exit. Calling preventDefault on this event does not revoke termination.
This is a notification, not a Promise that the OS process has already disappeared.

### Event object

| Field/method | Current contract |
| --- | --- |
| defaultPrevented | Boolean initially false; current objects are ordinary mutable JS objects |
| preventDefault() | Sets this.defaultPrevented to true when called on the event |

The same event instance is delivered synchronously to listeners for that emission.
Only before-quit/will-quit consume its cancellation flag. It is not the complete
Electron Event type: sender/frame/native-message properties are absent. Prefer
preventDefault over assigning fields, and keep cancellation before the first await.

## EventEmitter methods

app inherits the supported [node:events subset](mite.md), including
on/addListener, once, prependListener, removeListener/off and listener inspection.
Listeners run synchronously in registration order; thrown errors propagate through
the current application callback. App event names are case-sensitive strings.

EventEmitter can accept a listener for an arbitrary/custom name. That does not
create a native event producer. Manually emitting activate, for example, is not
OS activation support. Remove app-specific listeners when their owner is disposed;
a retained listener closure can retain its application's state.

## Unsupported Electron app surface

The [upstream app reference](https://www.electronjs.org/docs/latest/api/app) is a
migration inventory, not a declaration that these members exist in Firefly:

| Area | Examples still unavailable |
| --- | --- |
| App identity and paths | name, isPackaged, getName, setName, getVersion, getAppPath, getPath, setPath |
| Single-instance/relaunch | requestSingleInstanceLock, releaseSingleInstanceLock, hasSingleInstanceLock, relaunch |
| Protocol handlers | setAsDefaultProtocolClient, removeAsDefaultProtocolClient, isDefaultProtocolClient |
| OS integration | setUserTasks, setJumpList, addRecentDocument, clearRecentDocuments, dock, badgeCount |
| Appearance/media/process policy | commandLine, disableHardwareAcceleration, getGPUInfo, getAppMetrics, accessibility properties |
| Sessions and global services | session-created, certificate-error, login, child-process-gone and related native dispatch |
| Native lifecycle events | will-finish-launching, open-file, open-url, activate, did-become-active, did-resign-active, browser-window-created/focus/blur and OS shutdown delivery |

This table groups missing areas; it is not an exhaustive Electron member list.
Missing method calls currently produce ordinary JavaScript errors and missing
exports fail module import. The [example launcher](../../examples/electron/shared/run.mjs)
adds explicit capability/platform errors for reference-only demonstrations; it does
not install framework stubs. Assigning a missing app.name/isPackaged JS property
only changes a JS object and does not implement native metadata.

## Examples, changes and evidence

- [Lifecycle and forced exit](../examples/lifecycle.md): ESM source, cancellation trace and exit-code mode.
- [Deep links](../examples/deep-links.md): reference code and missing protocol/single-instance APIs.
- [Recent documents](../examples/recent-documents.md): native integration requirements.
- [Desktop launcher actions](../examples/desktop-launcher-actions.md): platform packaging contract.
- [BrowserWindow](browser-window.md) and [webContents](web-contents.md): object/loading lifetime.

Implementation: [firefly.js](../../firefly/core/api.mjs). Existing native/reference
evidence: [test_firefly.py](../../tooling/tests/test_firefly.py), especially lifecycle,
quit cancellation/reentry and forced-exit fixtures. Gallery evidence:
[test_electron_examples.py](../../tooling/tests/test_electron_examples.py). Commands and
individual sample coverage are in the [example catalog](../examples/index.md).

These four app methods and scoped lifecycle behavior are included in the first
[macOS ARM64 alpha](https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.1)
at 745c7af. Newer ESM example sources are subsequent master work; alpha.1 does not
contain them. This reference does not promise general Electron or Windows/Linux
GUI compatibility, notarization or a completed security review.
