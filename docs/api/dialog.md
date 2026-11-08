# dialog

Privileged main-process message boxes and file panels. Import `dialog` from
`firefly`, or from `electron` with `--electron-compat`. This API is distinct from
HTML file inputs: selected native paths belong to the trusted main process and are
never automatically published to a renderer.

The initial implementation uses Qt message boxes and macOS native open/save panels.
File selection on other platforms is explicitly unavailable. Native desktop and
controlled provider acceptance are separate; see validation below.

## Usage

```js
import { app, dialog } from 'firefly';
await app.whenReady();
const answer = await dialog.showOpenDialog({
  title: 'Open workspace',
  properties: ['openFile', 'openDirectory', 'multiSelections', 'treatPackageAsDirectory']
});
if (!answer.canceled) console.log(answer.filePaths);
```

An optional live `BrowserWindow` may be the first argument. The native Host verifies
that the ID still identifies a top-level window; child views, forged objects and
closed windows are rejected. A parent closing while a dialog is pending cancels its
native operation and rejects with `ERR_FIREFLY_OBJECT_DESTROYED`.

## Methods

| Method | Return value |
| --- | --- |
| `showMessageBox([window,] options)` | Promise of `{ response, checkboxChecked }`; response is the original zero-based button index. |
| `showMessageBoxSync([window,] options)` | Original button index, synchronously. |
| `showOpenDialog([window,] options)` | Promise of `{ canceled, filePaths }`; cancellation returns an empty array. |
| `showSaveDialog([window,] options)` | Promise of `{ canceled, filePath }`; cancellation returns `''`. |
| `showErrorBox(title, content)` | Synchronous error message, including before ready. Returns undefined. |
| `showOpenDialogSync`, `showSaveDialogSync`, other dialog APIs | Explicitly unsupported. |

All methods except `showErrorBox` require application readiness. All reject after
application exit. Call methods on the exported dialog singleton; borrowed receivers
are rejected. Options and arrays are copied before native admission; changing the
original request does not change the dialog. Native modal loops remain responsive
while main JavaScript pumping and bridge dispatch are deferred. A synchronous dialog
does not turn native UI processing into recursive application callbacks.

## Message options

Supported: `type` (`none`, `info`, `error`, `question`, `warning`), `title`, `message`,
`detail`, `buttons`, `defaultId`, `cancelId`, `checkboxLabel`, `checkboxChecked`,
`noLink` and `normalizeAccessKeys`. Detail is visible informative text, not HTML.
Buttons keep their original index even when native placement differs.

Absent or empty buttons use `['OK']`. The default button index is -1 unless provided.
Without an explicit cancel index, a case-insensitive Cancel/No button is preferred;
otherwise the index is1 when defaultId is0 and another button exists, or0. A close or
escape action selects the cancel index. The checked state belongs to that dialog; checkboxChecked:true requires a nonempty checkboxLabel.

Icons, AbortSignal and other unimplemented options fail explicitly. Firefly does not
silently ignore unsupported visual or security settings.

## File options

Common open/save options: `title`, `message`, `buttonLabel`, `defaultPath`, `filters`
and `properties`. Save also accepts `nameFieldLabel` and `showsTagField` on macOS.
A default path may describe an existing directory or a proposed filename; saving
returns a destination path and does not write the application's file data.

Filters are arrays of `{ name, extensions }`. Extensions have no leading dot or
wildcard; `'*'` means all files. On macOS, multiple filter groups use a native accessory selector; a selected group
limits allowed extensions, while an all-files group removes that restriction.

Open properties: `openFile`, `openDirectory`, `multiSelections`, `showHiddenFiles`,
`createDirectory`, `noResolveAliases`, `treatPackageAsDirectory`. macOS can select
files and directories together and traverse packages when requested. Omitting
properties defaults to openFile; an explicit list must include openFile or
openDirectory. Empty-selector lists are rejected rather than displaying an unusable panel.
Save properties: `showHiddenFiles`, `createDirectory`, `treatPackageAsDirectory`.

Security-scoped bookmarks, recent-document flags and other unsupported platform
properties are rejected. Returned paths are validated absolute native paths; open
selection may include directories, and a save destination need not exist. This API
does not read selected file bytes, create save files or grant renderer filesystem
permissions.

## Ownership, limits and errors

At most16 requests may be pending. IDs are monotonically increasing positive31-bit
integers, never reused. Native closing operations remain charged until completion.
At most256 selected paths and256KiB of result/request JSON are accepted. General
text is limited to32KiB UTF-8, buttons to32 entries of1024bytes, properties to16
entries, filters to64 groups with64 extensions each of256bytes. NUL and malformed
UTF-16 are rejected. Resource exhaustion throws `ERR_FIREFLY_DIALOG_LIMIT`.

Completions cross onto the VM owner thread as bounded plain data. The native layer
holds no JavaScript callbacks. The factory roots pending promises until settlement,
then revokes ownership. Duplicate, stale or canceled callbacks cannot settle a new
request. App/receiver/VM shutdown cancels native work, rejects pending promises and
permanently closes admission. Cleanup failures cannot retain application callbacks.

Other errors include `ERR_INVALID_ARG_TYPE`, `ERR_INVALID_ARG_VALUE`,
`ERR_OUT_OF_RANGE`, `ERR_FIREFLY_NOT_READY`, `ERR_FIREFLY_APP_EXITED`,
`ERR_FIREFLY_OBJECT_DESTROYED`, `ERR_FIREFLY_DIALOG_DATA`,
`ERR_FIREFLY_DIALOG_UNAVAILABLE`, `ERR_FIREFLY_DIALOG_NATIVE` and
`ERR_FIREFLY_UNSUPPORTED_API`.
A canceled selection is an ordinary result; infrastructure or unsupported-operation
errors are failures, never fabricated successful selections.

## Validation and differences

[Electron dialog](https://www.electronjs.org/docs/latest/api/dialog) provides the
signature/result baseline. Bounds, stricter unsupported-option rejection and
shutdown/parent-destruction errors are intentional differences. OS-specific styling,
accessibility and file panel behavior need native acceptance on each target platform.

Controlled tests cover the maintained ESM factory in actual Node/Mite, exact production
Host worker/GC/parent/shutdown behavior and native provider ownership. The unchanged
pinned Code-OSS message-option utility has a separate real-module consumer test;
this does not establish acceptance of its complete DialogMainService or workbench.

One bounded real Cocoa batch was attempted and stopped at its first message-result
assertion. No native file panel acceptance followed. The probe manipulated Qt backing
controls; Cocoa's actual NSAlert suppression state can overwrite those controls at
completion, so this attempt does not isolate a provider defect. Native message UI,
file selection and save-path acceptance remain unverified. Further visible tests are
postponed; passing controlled/offscreen tests must not replace these acceptance gates.

The exact offscreen QMessageBox gate passes all original button indices, checkbox
updates, finished/cancel ordering, parent destruction and synchronous execution.
It disables native helpers before application construction and has a hard process
deadline. It uses owned message text to identify widgets because macOS QMessageBox
intentionally ignores window titles. This is Qt widget logic acceptance, not Cocoa
NSAlert acceptance. The corrected native probe is compiled but has not been rerun.
