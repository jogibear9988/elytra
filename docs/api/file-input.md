# Native HTML file input

Availability: bounded native Qt file-input adapter wired into Firefly windows;
local macOS Release native-view/helper acceptance. Fresh Linux/Windows and
interactive operating-system sheet acceptance remain unverified.

```html
<input id="source" type="file" accept=".txt,.json" multiple>
<script>
  document.querySelector('#source').addEventListener('change', async event => {
    for (const file of event.target.files) {
      console.log(file.name, await file.text());
    }
  });
</script>
```

The HTML input uses the browser's activation/picker behavior. The native view opens
one nonmodal QFileDialog for the owning window. Production leaves Qt's platform
native-dialog selection enabled. Accept filters support extensions, MIME types and
audio/image/video groups with bounded filter processing; they are advisory and
include an All files option. `multiple` selects the single/existing-files mode.
No main-process dialog or preload binding is required to read selected browser Files.

## Selection and lifecycle

The main process opens each accepted path nonblocking, checks the opened handle is
a regular file and reads a bounded snapshot. Byte-backed SelectedFile IPC messages
carry the chosen basename and bytes to Ladybird's existing FileList/File creation.
Absolute paths are never substituted into the browser filename. The real helper
fixture verifies Unicode names and File.text() content across that IPC boundary.
File contents can change during a native read; this is a bounded byte copy, not an
atomic filesystem snapshot or a lazy file handle.

Selection limits are **16 files**, **4 MiB/file** and **16 MiB combined**. Empty files
are valid. Absolute native paths must be NUL-free and at most 4,096 UTF-8 bytes.
Directories, FIFOs/devices, missing/unreadable files and budget violations refuse
the entire batch; no earlier partial selection is delivered. A native diagnostic
records refusal and the renderer receives cancellation. Cancellation retains an
existing FileList according to the engine's file-input behavior.

Navigation, a new explicit load, helper crash and window close retire the chooser.
Finished callbacks from a cancelled dialog cannot publish a late accepted result.
A concurrent picker request cancels the outstanding request without delivering data.
Destruction drops completion before closing the dialog so it cannot call a dead view.

## Missing capabilities

This does **not** implement Electron's `webUtils.getPathForFile`. The pinned upstream
File/SelectedFile interfaces do not retain the native source path in a File object.
An internal [weak File/document provenance registry](../architecture/0147-native-file-provenance-registry.md)
and checked native input hook now pass local real-engine tests. The subsequent
[one-shot native request guard](../architecture/0148-native-file-request-ownership.md)
also passes in-process Page/File delivery tests. Actual trusted
chooser-to-helper path delivery and a narrowly validated File-object contextBridge
path are still required before the public API can be implemented. Deriving it from File.name, input.value or arbitrary
JavaScript properties would permit spoofing. No such inference is made here.

Directory upload, drag/drop provenance, general main-process dialog APIs, File clone
identity, large files, native file-handle/streaming transfer and measured low-overhead
file transport remain separate work. The picker currently copies complete bounded
file contents through IPC; it is not a zero-copy or performance acceptance result.

## Validation

`check-firefly-file-picker` uses real hidden Qt widget dialogs (native platform
sheets disabled only by an internal C++ test factory). It checks acceptance,
cancellation, stale finish, duplicate requests and destruction. Actual temporary
regular/empty/binary/Unicode files, a FIFO, directory and missing/invalid path cases
exercise exact/overflow count/file/aggregate budgets and basename-only results.

`check-firefly-file-input` uses an actual owned upstream SDK helper and Qt view. Native mouse
events activate a real file input; a hidden Qt dialog accepts a known file. The
result travels through existing SelectedFile IPC and the page checks FileList,
Unicode basename, private parent path absence and a 4 MiB exact-boundary file using
File.text() plus a full byte scan of File.arrayBuffer(), including an embedded NUL. A second request
cancels; navigation dismisses a third pending chooser and the new document has no
selected files. Windows/dialogs have DontShowOnScreen set: the test sends no mouse
or keyboard events to the user's desktop. The BrowserWindow adapter is compiled
and wired to the same chooser; this gate directly exercises the native view path,
not the production-helper/JavaScript application/Designer workflow or interactive OS sheet.

## JavaScript file and directory pickers

Firefly's second Ladybird patch adds `showOpenFilePicker()`,
`showSaveFilePicker()`, and `showDirectoryPicker()`. Call a picker from a
user gesture in an active secure document whose origin matches the top-level
document. Cancelling rejects the promise with `AbortError`.

```js
openButton.addEventListener('click', async () => {
  try {
    const [handle] = await window.showOpenFilePicker({
      types: [{ description: 'JSON', accept: { 'application/json': ['.json'] } }],
    });
    console.log(await (await handle.getFile()).text());
  } catch (error) {
    if (error.name !== 'AbortError') throw error;
  }
});

saveButton.addEventListener('click', async () => {
  try {
    const handle = await window.showSaveFilePicker({ suggestedName: 'export.json' });
    const stream = await handle.createWritable();
    await stream.write(JSON.stringify(data, null, 2));
    await stream.close();
  } catch (error) {
    if (error.name !== 'AbortError') throw error;
  }
});
```

| API | Current scope |
| --- | --- |
| `showOpenFilePicker()` | One or several files. Filters accept grouped extensions and native MIME mappings. |
| `showSaveFilePicker()` | One destination with optional suggested filename and filters. |
| `showDirectoryPicker()` | Read access by default. Pass `{ mode: 'readwrite' }` to select a folder for editing. |
| `FileSystemDirectoryHandle` | Async iteration with `entries()`, `keys()`, `values()`, and `for await…of`. Use `getFileHandle()` and `getDirectoryHandle()` with `{ create: true }` on writable folders. `removeEntry()` supports recursive removal. `resolve()` returns an array of relative components or `null`. |
| `FileSystemHandle` | Use `isSameEntry()` to compare native entries. `queryPermission()` reports the current permission and `requestPermission()` can prompt for write access after a user gesture. |
| `FileSystemFileHandle.getFile()` | A snapshot of up to 16 MiB with the file’s modification time and media type when recognized. A concurrent change can reject the snapshot. |
| `FileSystemFileHandle.createWritable()` | Writable handles support `write()`, `seek()`, `truncate()`, `close()`, `abort()`, and writer-based writes. `keepExistingData` copies up to 16 MiB. One writer can be open per handle. |

The browser retains the native paths and grants. Navigation retires grants
and discards staged writes. Closing a writer publishes its staged contents.
The named Desktop, Documents, Downloads, Music, Pictures, and Videos start
locations are available through `startIn`. A handle from the current document
can also provide the initial location. Each view remembers selections by
origin and picker ID until the view closes. Write grants are document-scoped.
Recursive removal stays on the selected filesystem and has a depth and entry
limit.

### Origin-private storage

In a secure same-origin window, `await navigator.storage.getDirectory()`
returns the writable root for that origin and browsing profile without a
picker. The root persists across navigation. Different origins use separate
roots. Native picker grants cannot enter the private backing directory.
The current limit is 64 MiB per origin, including staged writes.

#### Dedicated workers

A dedicated worker can open the same origin-private directory with
`navigator.storage.getDirectory()`. It can use asynchronous handles or call
`createSyncAccessHandle()` on an origin-private file for positioned reads
and writes. The default synchronous handle holds an exclusive lock. Close
the handle when finished so other writers can open the file.

```js
// In a dedicated worker
const directory = await navigator.storage.getDirectory();
const file = await directory.getFileHandle('data.bin', { create: true });
const access = await file.createSyncAccessHandle();
try {
  access.write(new Uint8Array([1, 2, 3]), { at: 0 });
  access.truncate(3);
  access.flush();
} finally {
  access.close();
}
```

`read()` and `write()` accept buffer sources and return the byte count.
`getSize()` returns the current size. `truncate()` and `flush()` update
the file directly. Explicit offsets are optional. The handle otherwise
uses its current file position. Use `{ mode: 'read-only' }` to open a
shared read-only handle.

#### Saving and sharing origin-private handles

Origin-private file and directory handles support `structuredClone()`,
same-origin `postMessage()`, and IndexedDB. The receiving environment
opens a fresh capability on its first filesystem operation. An IndexedDB
handle can be read after navigation. Deleting the underlying entry makes
operations that reopen it fail with `NotFoundError`. A different origin
cannot deserialize the handle.

#### Restoring native picker handles

Handles returned by native pickers can also be cloned or stored in
IndexedDB. The browser stores the selection in the browsing profile and
binds the reference to the selecting origin. A restored handle initially
needs permission. In a Window, call `requestPermission()` during a
user gesture to approve reading. Pass `{ mode: 'readwrite' }` to request
editing. Select **Remember this permission** in the browser prompt to keep
that decision for the origin and location. Otherwise the approval applies
to the current document. A same-origin worker can use a handle whose
permission has been remembered. Native paths stay inside the browser.

### Remaining compatibility

The native file-grant provider is verified on macOS. Windows support,
private-browsing storage, native drag-and-drop handles, and runtime coverage
for Linux, shared workers and service workers remain open. The compatibility
surface is tracked against the
[File System Standard](https://fs.spec.whatwg.org/) and the
[File System Access draft](https://wicg.github.io/file-system-access/).
