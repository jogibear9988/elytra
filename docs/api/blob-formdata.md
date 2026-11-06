# Blob, File and FormData

Status: partial native-runtime data APIs, tested locally on macOS against actual
Node 24.15.0. Fresh Linux/Windows acceptance is pending. These are Mite CLI/Firefly
main-process globals; Ladybird renderer objects remain separate. Blob and File
are also the same constructors exported by CommonJS/ESM `node:buffer`.

```js
const form = new FormData();
form.append('project', 'Mite + Firefly');
form.append('file', new File(['{"runtime":"LibJS"}'], 'project.json', {
  type: 'application/json'
}));
const response = await fetch('http://localhost:8080/upload', {
  method: 'POST', body: form
});
console.log(response.status);
```

## Immutable Blob bytes

`new Blob(parts = [], options?)` accepts an iterable object of strings,
ArrayBuffer/views and supported Blobs/Files. Byte views respect offset/length and
copy their input: changing the caller's buffer later does not change the Blob.
Immutable Blob parts can be shared. Other values convert to scalar strings;
symbols and invalid part sequences reject. Unpaired surrogates become U+FFFD;
an initial BOM remains part of stored content.

| Member | Contract |
| --- | --- |
| `size` | Stored byte length, not character count |
| `type` | Printable ASCII MIME string normalized to lowercase; invalid MIME characters produce empty type |
| `slice(start?, end?, type?)` | Blob containing the selected byte range; negative bounds count from the end; fractional indices truncate; slices of Files are Blobs |
| `arrayBuffer()` | Promise of independent byte storage |
| `bytes()` | Promise of an independent Uint8Array |
| `text()` | UTF-8 decode with replacement and BOM handling |
| `stream()` | Fresh byte ReadableStream for each call; default/BYOB readers supported |

The stream adapter pulls chunks of at most 64 KiB and supports cancellation and
zero-byte EOF for an outstanding BYOB reader. Read/cancel affects that reader,
not the immutable Blob or future streams. Retain BYOB results because input buffers
transfer. Codecs/stream constructors are captured; replacing globals does not
change existing Blob and Fetch behavior, including internal slice readers.

`options.type` supplies MIME metadata. `options.endings` accepts transparent
(default) or native. Native follows the measured Node newline conversion on the
host, preserving standalone CR while converting LF/CRLF. Nonfinite slice indices
also follow Node 24's conversion to zero. These details are not claims of complete
File API/WebIDL conformance. Shared/resizable/detached buffers, all numeric coercion
edges, huge allocations and cross-realm providers need further coverage.

## In-memory File metadata

`new File(parts, name, options?)` is a Blob with readonly `name` and `lastModified`.
Options include the Blob options and lastModified (default Date.now). Names are
scalar strings. Timestamp conversion follows the Node reference, including its
fractional values and NaN-to-zero behavior. Invalid receivers/required arguments
throw TypeError.

A File contains bytes supplied by the application. Its name does not read or
authorize an OS path. Filesystem-backed/openAsBlob factories and filesystem streams
are still absent. To use a local file, explicitly read bytes through the supported
[filesystem API](mite.md#nodefs-and-nodefspromises) and construct File. There is no Blob URL/object
URL registry, automatic Firefly IPC transfer or renderer filesystem access here.

## Ordered FormData entries

`new FormData()` creates an empty ordered entry list. HTMLFormElement constructors
are unavailable in the native main process. A value is a scalar string or File;
appending a Blob creates a File named blob unless a filename is supplied.

| Method | Behavior |
| --- | --- |
| `append(name, value, filename?)` | Add an entry without removing earlier values |
| `set(name, value, filename?)` | Replace entries at their first existing position |
| `delete(name)` | Remove every entry for the name |
| `get(name)` / `getAll(name)` | First value or null / fresh array of all values |
| `has(name)` | Boolean presence |
| `entries()` / iterator, `keys()`, `values()` | Ordered live iteration |
| `forEach(callback, thisArg?)` | Value/name/owner callback with optional receiver |

An existing File retains identity when no filename override is supplied. Renaming
creates a File with the same immutable content/type/lastModified. A filename for a
non-Blob value rejects TypeError. String names/values and filenames normalize lone
surrogates while preserving BOM content. Methods enforce required arguments and
receiver brands; exact prototype flags/error text/overload parity is not complete.

## Fetch bodies and multipart framing

Request/Response accept Blob, File or FormData without `duplex: 'half'`. These are
replayable immutable body sources. A Blob's nonempty type supplies Content-Type
unless overridden. FormData extraction snapshots its entries into an immutable
multipart Blob; later edits to the original list do not change that body. File
content is read incrementally from Blob parts, without collecting the entire file
into a new multipart byte array first.

Multipart boundaries use the existing native crypto CSPRNG. The serializer escapes
quotes/CR/LF in names and filenames and normalizes text field line endings to CRLF.
It computes exact byte Content-Length from immutable parts. Do not manually supply
a mismatched multipart Content-Type/boundary. A 307/308 replay retains the same body
and header boundary. Tests preserve a measured Node 24.15 defect separately: its
FormData redirect can produce a new body boundary with the previous header, which
the independent MIME peer rejects. Mite's replay remains parseable and byte-identical.

`request.blob()` / `response.blob()` consume the body and return Blob with parsed,
serialized MIME metadata. Invalid Content-Type gives an empty Blob type. Existing
Body ownership/locking, clone and abort rules apply.

`request.formData()` / `response.formData()` consume once and parse either
application/x-www-form-urlencoded or multipart/form-data. URL fields use UTF-8 and
preserve duplicates, plus signs, replacement decoding and a leading BOM in names.
Multipart preserves entry order and File names/types/bytes, including UTF-8 paths
and tested base64 file transfer encoding. Invalid/missing boundaries, malformed
bodies and unsupported Content-Type reject TypeError after consumption.

These convenience consumers collect their results in memory. The multipart parser
uses no silent field/file truncation; a tested 1.1-million-byte field stays intact.
The parser retains fixed header validity/size checks. Every legacy per-part charset,
disposition/transfer encoding, malformed boundary and cancellation/GC edge is not
covered; full WHATWG/Web Platform Tests conformance is not claimed.

## Implementation and validation

The immutable byte core is pinned fetch-blob 3.2.0 (MIT), with owned ESM adapters for
host streams/codecs, MIME normalization, native endings and public classes. Its
optional Node/old-stream fallback is replaced by an owned build-resolution adapter;
package sources stay unmodified. Only the existing Web Stream provider is shipped,
not a second polyfill or node-domexception shim. Multipart/MIME parsing reuses
busboy 1.6.0, streamsearch 1.1.0 and whatwg-mimetype 4.0.0, all pinned with source/
integrity metadata and original MIT notice wording with canonical LF endings. Multipart parser loading
is lazy. No Node, V8 or LibWeb runtime is required.

Buffer utf8Slice/latin1Slice/ucs2Slice/base64Slice codec helpers support the parser;
48 actual Node comparisons check their bounds/coercion and decoding. The data suite
adds 13 object workflows plus five independent HTTP/MIME workflows: binary and empty
uploads, snapshots, lengths/hash, Blob replay, large fields, base64 Files, response
consumers and the ESM upload sample. A separate redirect diagnostic records both
Node and Mite outputs rather than normalizing the boundary defect into parity.

Run the [modern multipart example](../../examples/mite-fetch/multipart.mjs) with an
explicit upload endpoint. Native platform, full File/WebIDL/WPT, Blob URL registry,
filesystem-backed files and transferable object coverage remain open. Compression
and other [Fetch policy limits](fetch.md) are separate work.

References: [Fetch standard](https://fetch.spec.whatwg.org/),
[fetch-blob](https://github.com/node-fetch/fetch-blob),
[busboy](https://github.com/mscdex/busboy).
