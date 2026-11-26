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
The complete Code-OSS preload and workbench still do not run.

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
| `FileSystemFileHandle.getFile()` | A snapshot of up to 16 MiB. |
| `FileSystemFileHandle.createWritable()` | Writable handles support `write()`, `seek()`, `truncate()`, `close()`, `abort()`, and writer-based writes. `keepExistingData` copies up to 16 MiB. One writer can be open per handle. |

The browser retains the native paths and grants. Navigation retires grants
and discards staged writes. Closing a writer publishes its staged contents.
The named Desktop, Documents, Downloads, Music, Pictures, and Videos start
locations are available through `startIn`. A handle from the current document
can also provide the initial location. Each view remembers selections by
origin and picker ID until the view closes. Write grants are document-scoped.
Recursive removal stays on the selected filesystem and has a depth and entry
limit.

Persistent permissions, structured cloning, origin-private storage, and
synchronous worker handles remain open. Windows has no native
file-grant provider. The compatibility surface is tracked against the
[File System Standard](https://fs.spec.whatwg.org/) and the
[File System Access draft](https://wicg.github.io/file-system-access/).
