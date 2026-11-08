# shell

Status: **partial macOS native desktop integration**. Production policy passes in
Node/Mite, and the exact native Host and macOS provider pass controlled component
gates. A separate opt-in test passes genuine OS Trash of one owned temporary
Unicode file and restoration from the exact returned destination. No browser,
application or Finder launch was tested. Native Windows/Linux providers are
unavailable; actual visible launch, activation and selection remain unaccepted.

`shell` is a main-process singleton exported by `firefly`, including through the
explicit Electron compatibility alias. It is absent from ordinary renderer
capabilities and standalone Mite. The methods do not require app readiness, but
fail after application exit.

```js
import { shell } from 'firefly';

const message = await shell.openPath('/absolute/path/to/report.pdf');
if (message) {
  console.error(message);
}

await shell.openExternal('https://example.com', { activate: false });
```

These calls cause desktop actions. Applications should select trusted paths and
URLs appropriate for the requested action before passing them to this API.

## Methods and results

The public shapes follow the [Electron shell API](https://www.electronjs.org/docs/latest/api/shell).

| Method | Contract |
| --- | --- |
| `showItemInFolder(path)` | Synchronous `undefined` result after native submission. It requests reveal/select in the file manager; submission does not prove a visible file-manager window. |
| `openPath(path)` | `Promise<string>`: `''` on native success, or the ordinary OS operation's error message on failure. Infrastructure failures reject instead. |
| `openExternal(url, options?)` | `Promise<void>`: resolves after native completion or rejects with an operation/infrastructure error. |
| `trashItem(path)` | `Promise<void>`: resolves after native completion or rejects on failure. This requests an OS Trash move, not permanent deletion. |

The `openExternal` options snapshot supports `activate`, a strict boolean whose
default is `true`. This option controls macOS activation. Windows options
`workingDirectory` and `logUsage` are explicitly unsupported, including when
provided with their default values. Unknown options and symbol keys are rejected;
they are not silently ignored. Native platform availability remains separate
from JavaScript option validation.

`beep`, `writeShortcutLink` and `readShortcutLink` throw
`ERR_FIREFLY_UNSUPPORTED_API`.

## Inputs and compatibility boundaries

Paths and URLs must be nonempty strings. Path inputs have a 32768-byte UTF-8
budget; URLs have a 65536-byte budget. NUL and unpaired UTF-16 surrogates are
rejected rather than truncated or replaced during native conversion. Native
path handling captures an absolute path during admission, before work is
dispatched asynchronously. It preserves dot segments and symlinks; no lexical
cleaning or canonicalization changes which item the OS operates on. Foundation
file URLs can expose canonically equivalent decomposed Unicode on macOS. Pass an absolute path when a stable caller-selected
location is required.

Methods require their original `shell` receiver. Option values are read once and
copied before admission, with lifecycle checks after getters and native calls.
Asynchronous methods return rejected promises for invalid inputs; synchronous
`showItemInFolder` throws. These strict input rules, bounds and explicit codes
are Firefly policy; identical Electron coercion, error classes, wording and
argument-error timing are not claimed.

## Admission, completion and shutdown

At most 64 JavaScript requests can be pending. Request IDs are positive,
monotonic signed-int32 integers and are never reused. Quota or ID exhaustion
rejects with `ERR_FIREFLY_SHELL_LIMIT`. A native admission exception rejects the
promise, even for `openPath`. A synchronous native callback is held until native
admission returns successfully; an admission exception overrides that callback.
The first completion wins, and duplicate or unknown/stale IDs are ignored.

For `openPath`, only a completed ordinary OS error becomes a resolved message.
Unavailable services, quota failures, malformed data, application exit and
unsupported API failures still reject. Native operation errors preserve their
message and code when available. Malformed current-request completions reject
with `ERR_FIREFLY_SHELL_DATA` and retire that delivery slot without revoking other
requests. Success records cannot also carry error fields, and native error
messages must contain valid UTF-16 within their UTF-8 budget.

There is no operation timeout: an OS launch or Trash operation cannot reliably
be cancelled. Application shutdown permanently revokes the facade, rejects all
pending promises with `ERR_FIREFLY_APP_EXITED`, and requests native delivery
cancellation/shutdown. Late native results cannot resettle those promises.
Cancellation only suppresses delivery; it cannot roll back an application launch,
file reveal or Trash move already submitted to the OS. Native OS work retains its
own bounded ticket until its real callback retires, even if JavaScript delivery
was cancelled.

## Validation scope

`firefly/tests/shell/policy.mjs` and `tooling/tests/test_shell_policy.py` exercise
the actual factory with a controlled peer in Node and Mite. Accepted checks cover
input and receiver validation, UTF-8 bounds, option snapshots, result shapes,
early callbacks, admission failure, quotas, duplicate/stale/malformed results,
and reentrant shutdown. No Electron shell action oracle is invoked.

The [exact Host fixture](../../firefly/tests/shell/HostProbe.cpp) passes native
validation, worker-to-owner delivery, synchronous callback queuing, Unicode/error
bounds, quotas, actual GC and controller/receiver/VM teardown. It links a controlled
provider and never calls an OS shell service.

The [macOS provider fixture](../../firefly/tests/shell/ProviderProbe.mm) intercepts
desktop singleton methods before any provider call and never invokes the original
launch/reveal/trash methods. It passes actual native URL/configuration metadata,
activation flags, failures, worker trash dispatch, global 64-request accounting,
late completion and stop behavior. Cancellation does not refund an unfinished OS
callback ticket. These checks establish provider state, not visible OS effects.

The opt-in [native Trash probe](../../firefly/tests/shell/NativeTrashProbe.mm)
passes genuine Trash and exact-destination restoration of one uniquely owned
Unicode temporary file, including byte verification. It reads the resulting URL
from that one operation; it never enumerates Trash or touches unrelated files.
It does not open an application or file manager.

The [actual unavailable provider](../../firefly/tests/shell/UnsupportedProbe.cpp)
is also exercised without Qt or a desktop. This proves explicit refusal and
permanent stop, not native Windows/Linux platform acceptance.

The release build runs non-GUI service gates before the separate desktop/package
batch; Linux renderer CI includes controlled dispatch and unavailable-provider
gates. Fresh hosted outcomes require their own acceptance evidence. Actual launch,
activation, Finder selection, OS launch/trash failure races and native Windows/
Linux integration remain separate platform work.

The actual pinned Code-OSS local-desktop/NLS ready diagnostic passes the shell
export and next fails linking `Electron dialog`. Entry/app-ready observation
is not acceptance of a VS Code window or open/edit/save workflow. See
[VS Code status](../compatibility/vscode.md).
