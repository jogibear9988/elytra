# clipboard and ClipboardItem

Copy and read native desktop text from the main process.

**Availability:** partial macOS implementation. The native private-pasteboard
probe passes on macOS ARM64; the actual API factory passes in Node and Mite
without a graphical application. Public Electron/Firefly acceptance is added to
the hosted desktop suite and is pending execution. Native Windows/Linux desktop
adapters remain unfinished. This API is not exported by standalone Mite,
preloads, contextBridge or document JavaScript.

The compatibility target is the pinned
[Electron 45.2.3 clipboard API](https://github.com/electron/electron/blob/v45.2.3/docs/api/clipboard.md)
and [ClipboardItem](https://github.com/electron/electron/blob/v45.2.3/docs/api/clipboard-item.md).
That version uses asynchronous reads/writes and MIME-typed items. Older Electron
examples using synchronous `readText`, `writeHTML`, `readRTF` or the old
`write({text, html})` shape require migration.

```js
import {clipboard, ClipboardItem} from 'firefly';

await clipboard.write([
  new ClipboardItem({
    'text/plain': 'Hello',
    'text/html': new Blob(['<b>Hello</b>'], {type: 'text/html'})
  })
]);
console.log(await clipboard.readText());
```

ESM and CommonJS expose the same initial clipboard singleton and ClipboardItem
constructor. Clipboard access has no app-readiness guard. It acts on the general
macOS pasteboard, shared with other applications. No automatic renderer access,
IPC action or browser clipboard permission is introduced.

## clipboard methods

| Member | Return | Behavior |
| --- | --- | --- |
| `readText()` | `Promise<string>` | Read native plain text as UTF-8, empty string if absent |
| `writeText(text)` | `Promise<void>` | Replace the clipboard with one plain-text representation |
| `read()` | `Promise<ClipboardItem[]>` | Snapshot supported representations of native entries |
| `write(items)` | `Promise<void>` | Publish zero or one ClipboardItem after resolving all payloads |
| `has(mimetype)` | `Promise<boolean>` | Check native representation metadata for a supported MIME type |
| `clear()` | `undefined` | Synchronously clear the general pasteboard |
| `selection` | `undefined` | No Linux selection clipboard on macOS |

The supported MIME types are exactly `text/plain`, `text/html` and `text/rtf`.
HTML is stored as raw UTF-8 bytes without an inserted charset header. RTF Blob
payloads retain raw bytes; string payloads encode UTF-8. Plain text supports
Unicode, line breaks and embedded NUL. AppKit's external UTF-16 text format is
converted to UTF-8 and its automatic alias is deduplicated when UTF-8 is present.

`read()` snapshots bytes independently of later OS changes. It rejects if a
native item contains an unsupported format, rather than returning a successful
partial item. `readText()` can still read a supported text representation when
other formats coexist. External applications can change the clipboard between
calls; there is no ownership lock across a sequence of queries. A changed native
generation during a multi-format read rejects with
`ERR_FIREFLY_CLIPBOARD_CHANGED`.

`write([])` clears. Writing more than one entry rejects; multiple representations
of one entry are supported. All payloads resolve and validate before any native
ownership change. Failed promises, invalid payloads, unsupported types and budget
failures during preparation leave existing data intact. Native publication uses
AppKit's clear/writeObjects protocol: an OS publication failure is reported and
can leave the clipboard empty. There is no rollback or guarantee against another
application replacing the data immediately afterward. Concurrent asynchronous
writes are not serialized; a slower promised payload can publish later.

## ClipboardItem

### new ClipboardItem(items)

`items` is a nonempty record of MIME type keys and string, Blob or promised
string/Blob payloads. The constructor snapshots keys and payload references.
Strings encode UTF-8; Blob data is immutable. Payload promises resolve when
`getType()` or `clipboard.write()` requests them, not at construction time.

### item.types

A read-only getter returning a fresh array of MIME keys. Mutating that array
does not alter the item. No presentation-style option or static supports method
is implemented. Constructor/subclass/coercion behavior is not generally
identical to Electron's built-in class.

### item.getType(type)

Returns `Promise<Blob>`. Rejects if the key is absent or the payload does not
resolve to a string/Blob. A rejected payload retains its rejection reason.
This class can hold an unsupported MIME key, but native `write()` rejects it.
Bookmark records are not implemented. A supplied Blob retains its own type;
string and native-read payloads produce a Blob with the representation's MIME.

## Errors and limits

| Code | Condition |
| --- | --- |
| `ERR_INVALID_ARG_TYPE` | Non-string writeText, invalid item/payload |
| `ERR_INVALID_ARG_VALUE` | Invalid/empty record, missing item type or write array shape |
| `ERR_FIREFLY_UNSUPPORTED_API` | Unsupported requested/native format |
| `ERR_FIREFLY_CLIPBOARD_LIMIT` | Combined representation data exceeds eight MiB |
| `ERR_FIREFLY_CLIPBOARD_UNAVAILABLE` | Native pasteboard/provider/publication or UTF-16 decoding failure |
| `ERR_FIREFLY_CLIPBOARD_CHANGED` | Clipboard generation changes during read |

The eight-MiB limit applies per operation across representations; UTF-16 source
bytes are also checked before conversion. The private host wire has a separate
16-MiB UTF-16-unit cap. OS providers may allocate or block while fulfilling a
read; these checks are not a total process memory limit or an asynchronous OS
worker pool. Native clipboard work currently executes on the application thread;
the public API returns promises for Electron compatibility and payload resolution.

Images, files/URI lists, bookmarks, find-text, arbitrary MIME/custom/raw formats,
clipboard events and Windows/Linux selection integration are not implemented.
Unsupported format requests reject explicitly. Other absent members produce
ordinary JavaScript API errors. Full provider/race/error-shape and cross-app
interop coverage remains open.

## Example and validation

The [modern example](../../examples/clipboard/main.mjs) reads text by default;
`--copy` explicitly replaces it with a text/HTML entry:

```sh
build/renderer/bin/firefly examples/clipboard/main.mjs --copy
```

Local acceptance uses a uniquely named pasteboard and never touches the general
clipboard or launches a GUI app:

```sh
cmake --build build/renderer --target firefly-clipboard-probe --parallel 3
python3 tooling/tests/test_clipboard.py --mite build/release-mite/bin/mite --probe build/renderer/bin/firefly-clipboard-probe
```

The native probe verifies stored bytes using independent AppKit reads, external
UTF-16 text, unsupported/malformed/duplicate/oversized preparation and clear.
The shared factory suite checks snapshot ownership, promises/rejection identity,
byte preservation, validation and native error propagation. The public
[desktop fixture](../../firefly/tests/migration/clipboard.mjs) modifies the
general clipboard only on CI, with explicit fixture opt-in. Local desktop suites
report that fixture as not run; native private-board tests remain available.
