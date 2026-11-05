# app

Availability: Firefly main process only; partial implementation tested on native
macOS ARM64. Import the shared EventEmitter singleton from firefly. There is no
renderer app export or public electron alias.

```js
import {app, BrowserWindow} from 'firefly';
await app.whenReady();
const window = new BrowserWindow({width: 1000, height: 700});
await window.loadFile('index.html');
app.on('window-all-closed', () => app.quit());
```

## Readiness

| Signature | Return / contract |
| --- | --- |
| app.isReady() | boolean; initially false, true once native readiness is observed |
| app.whenReady() | Shared Promise, resolves with undefined after ready listeners run |

Event ready has no arguments. Register listeners before readiness or await
whenReady to handle either state. BrowserWindow construction before readiness
throws ERR_FIREFLY_NOT_READY; construction after app exit throws ERR_FIREFLY_APP_EXITED.

## app.quit()

No parameters; returns undefined. Requests the normal cancellable shutdown flow:

1. Emit before-quit(event). preventDefault cancels this attempt.
2. Request each live window's close. A cancelled close stops that quit attempt.
3. Once all windows have closed, emit will-quit(event). preventDefault cancels exit.
4. Emit quit(event, 0) and request native termination.

Reentrant quit requests coalesce while an attempt is active. Calling quit again
after cancellation starts a new attempt; already closed windows stay closed.
window-all-closed is emitted when the last window closes outside an active quit;
it is suppressed during normal/forced quit. Decide the last-window policy explicitly.

Cancellation must happen synchronously in the listener. EventEmitter does not await
async listeners. beforeunload and full platform activation/dock/Cmd-Q parity are not
implemented. Use the [lifetime guide](../concepts/async-and-lifetime.md) for resources.

## app.exit([exitCode])

| Parameter | Type / default |
| --- | --- |
| exitCode | Integer number, default 0; signed 32-bit range |

Returns undefined. Bypasses before-quit, will-quit and cancellable window close.
Destroys live windows, rejects pending navigation with ERR_ABORTED, emits closed
and quit(event, exitCode), then requests termination. It suppresses window-all-closed.
The current JavaScript turn can finish before native termination. Repeated calls
after exit do nothing, following argument validation.

Noninteger/nonnumber values throw ERR_INVALID_ARG_TYPE; values outside signed int32
throw ERR_OUT_OF_RANGE. Native exit-code representation can differ by OS.

## Events and event objects

| Event | Arguments / role |
| --- | --- |
| ready | No arguments; readiness observed |
| window-all-closed | No arguments; last window closed outside an active quit |
| before-quit | event; cancellable start of normal quit |
| will-quit | event; cancellable final stage after windows closed |
| quit | event, numeric exitCode; termination requested |

Lifecycle event objects have defaultPrevented and preventDefault(). Only
before-quit/will-quit use cancellation here; a preventDefault call on quit cannot
undo termination. They are small project event objects, not complete Electron events.

## Missing surface and evidence

Paths, single-instance locking, application menus, packaged metadata, protocol
registration and most Electron app methods are absent. Calling an absent method
produces an ordinary JavaScript error; unsupported exports are not success stubs.
Windows/Linux native desktop availability is unverified.

Implementation: [firefly.js](../../firefly/core/firefly.js).
Validation: [Firefly differential runner](../../tooling/test_firefly.py), lifecycle
fixtures and [native build gates](../development/renderer-build.md).
See [compatibility](../compatibility/firefly-0.1.md) and [BrowserWindow](browser-window.md).
