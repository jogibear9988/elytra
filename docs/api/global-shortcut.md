# globalShortcut

Main-process system-wide keyboard shortcuts. Import `globalShortcut` from `firefly`,
or from `electron` with the explicit `--electron-compat` alias.
Mite itself has no desktop shortcut API. Renderer and utility processes do not expose it.

The initial provider uses macOS Carbon hotkey registrations on the native Cocoa
backend. Linux and Windows providers are unavailable and fail explicitly; offscreen
macOS admission is refused. This is not a Qt application-local shortcut fallback.

## Usage

```js
import { app, globalShortcut } from 'firefly';

await app.whenReady();
const registered = globalShortcut.register('CommandOrControl+Shift+F19', () => {
  console.log('Shortcut pressed');
});
if (!registered) console.log('Shortcut unavailable');
app.on('will-quit', () => globalShortcut.unregisterAll());
```

Registration changes system-wide keyboard ownership. Select accelerators deliberately
and handle `false`; another application or the operating system may own them.

## Methods

| Method | Result and behavior |
| --- | --- |
| `register(accelerator, callback)` | Boolean admission result. Invokes the callback with no arguments on the main JavaScript thread. Duplicate owned or conflicting physical chords return false. Native infrastructure failures throw a coded error. |
| `registerAll(accelerators, callback)` | Boolean transactional result. A conflict rolls back registrations from this call; earlier registrations remain owned. Exceptions also roll back the new batch. |
| `isRegistered(accelerator)` | Boolean query of this API's successful, canonicalized registrations. Does not query other applications or infer ownership of physically equivalent spellings. May run before ready. |
| `unregister(accelerator)` | Returns undefined. Retires the callback and releases the owned registration; unknown shortcuts are harmless. |
| `unregisterAll()` | Returns undefined. Retires all callbacks and attempts every owned removal, then throws the first native failure if any. |
| `setSuspended(suspended)`, `isSuspended()` | Explicitly unsupported. |

Methods other than `isRegistered` require `app.ready`. All methods reject after
application exit. Call methods on the exported singleton; borrowed receivers are
rejected. Callbacks may unregister, register other shortcuts or exit the application.
Callback exceptions follow ordinary main-process event-loop error handling.

## Accelerators

Supported modifiers: Shift, Control/Ctrl, Alt/Option, Command/Cmd/Meta/Super and
CommandOrControl/CmdOrCtrl. The latter selects Command on macOS and Control elsewhere.
Modifier names and named keys are case-insensitive.

Keys: A–Z, 0–9, F1–F20, Space, Tab, Enter/Return, Escape/Esc, Backspace, Delete,
Insert, Home, End, PageUp, PageDown, arrows, and printable ASCII punctuation.
Use `Plus` for the literal plus key, for example `Control+Plus`.
Exactly one non-modifier key is required. Text is bounded to 4096 characters.

macOS resolves character keys against the selected TIS Unicode keyboard layout at
registration time, including required Shift/Option layers. It does not assume a US
keyboard. Native physical collisions are checked even when spellings differ. Missing
or unmappable layouts fail explicitly. Live remapping after changing the input source
is not implemented: unregister and register again. IME-only input sources without a
Unicode keyboard layout may therefore be unavailable.

Media, volume, AltGr, numpad, CapsLock, ScrollLock, PrintScreen and F21–F24 are
unsupported. Ordinary Carbon registration does not request accessibility permission;
media-key accessibility support is not implemented.

## Ownership, limits and errors

A singleton owns at most 64 live registrations; native macOS ownership is also bounded
to 64 process-wide handles, including retired handles whose native cleanup failed.
Tokens are monotonically increasing positive 31-bit integers and never reused.
`registerAll` accepts at most 64 accelerators. Exhaustion throws
`ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT`.

Callbacks remain rooted while owned. Native callbacks carry integer tokens, never
LibJS objects. Events are queued onto the owner thread and discarded after
unregister, receiver destruction or application/VM shutdown. One pending event per
owned token is retained while the owner is busy; repeated held-key events may be
coalesced. This API does not guarantee every hardware repeat.

Removal revokes callback delivery before native cleanup. A failed OS removal throws
`ERR_FIREFLY_GLOBAL_SHORTCUT_NATIVE`; its inactive handle remains charged to the
native budget and is retried during subsequent admission, unregisterAll or shutdown.
A persistent release failure blocks new admission. Such a failure
can retain an OS grab until retry or process exit, but cannot deliver a retired
callback. Cleanup is limited to this provider's handles.

Other failures include `ERR_INVALID_ARG_TYPE`, `ERR_INVALID_ARG_VALUE`,
`ERR_OUT_OF_RANGE`, `ERR_FIREFLY_NOT_READY`, `ERR_FIREFLY_APP_EXITED`,
`ERR_FIREFLY_GLOBAL_SHORTCUT_LAYOUT`, `ERR_FIREFLY_GLOBAL_SHORTCUT_UNAVAILABLE`
and `ERR_FIREFLY_UNSUPPORTED_API`.

## Compatibility and validation

[Electron's API](https://www.electronjs.org/docs/latest/api/global-shortcut) is the
signature/readiness/ownership baseline. Firefly adds bounded resources, strict single-key
parsing, a transactional Boolean `registerAll`, explicit unavailable-platform errors
and the repeat coalescing described above. Do not infer current Electron suspension,
Wayland portal or media support from these method names.

Maintained gates separate JavaScript policy, exact native Host delivery/GC/teardown,
and the exact macOS provider with controlled Carbon/TIS peers. The pinned original
Code-OSS GlobalKeybindingsMainService consumer uses its constructor injection seam
for a controlled registry while importing genuine Firefly desktop modules. These
checks do not prove physical keyboard delivery, a VS Code workbench or Windows/Linux
shortcut support. The optional real-OS registration probe performs registration and
cleanup only, without generating key events.
