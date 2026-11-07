# Native regular-file response bodies

This is the main-side body component for future protocol file responses. It is
not an exported protocol handler or URL-to-path resolver. Trusted native code must
already have authorized the path returned by the application's handler. No renderer
path, root, request or descriptor is admitted by this component itself.

`FileBody::open` takes an absolute path, checks the opened descriptor is a regular
file, and captures its size. Paths are bounded to 32,768 bytes and reject NUL;
files larger than 64 GiB fail explicitly. POSIX opens nonblocking before checking
the descriptor so a FIFO cannot stall admission. Windows checks for a disk handle
and a regular file. Symlinks may resolve to a regular file: authorization, root
containment and symlink policy remain the caller's responsibility.

One owned descriptor serves all reads; chunks never reopen the path. A caller
pulls at most 64 KiB per read and controls scheduling/backpressure. Empty reads
mean complete body delivery. HEAD preserves content length without a body. EOF,
HEAD, cancellation, errors and destruction close the descriptor. Cancellation is
idempotent and subsequent reads fail. Premature EOF after file truncation is an
error; growth cannot exceed the originally captured length. This is a bounded live
file reader, not an immutable content snapshot. Concurrent edits within that length
can change bytes. Reads are synchronous; asynchronous IO/worker scheduling belongs
to the future request service.

ResponseHead adds a versioned, exact internal file-response schema with a nonzero
32-bit request ID, status 200, captured length, HEAD flag and ordered header pairs.
The encoded head is limited to 32 KiB / 64 header pairs. Body bytes remain on the
raw byte channel. No implicit source path, URL or descriptor is serialized.

The main-side builder derives Content-Type from upstream Core's MIME table unless
the handler supplies it. Content-Length is generated from the actual opened file;
application overrides and duplicate type/length fields fail. Preserve other header
order and duplicates. Reuse upstream HTTP field validation and refuse network
framing/hop-by-hop fields. HTTP values use an explicit isomorphic byte-to-Unicode
mapping in metadata so obs-text bytes survive JSON without accidental UTF-8
expansion in the reconstructed HTTP value. Check both raw and encoded budgets.
The decoder repeats identity/type/header/length checks and refuses unknown schema,
keys and non-byte header characters.

This carries Code-OSS's COOP/COEP, cache and Document-Policy headers, but does not
apply their browser security behavior. Cookie filtering/storage, CSP/CORS/isolation
and response application belong to renderer fetch integration. Only a successful
200 file response is represented; errors, ranges, redirects and other statuses
need explicit request/response service support.

```sh
cmake --build build/renderer --target check-firefly-protocol-files --parallel 3
```

The native gate reads a real 4 MiB + 19 byte file and streams 65 bounded chunks over
the existing byte channel to a separate native child. The child admits and checks the real response metadata before any body bytes,
then independently checks all bytes and returns its kernel PID and cumulative position. It never
receives the source path. The probe also checks empty files, HEAD, chunk budgets,
cancellation, growth, descriptor identity across path replacement, truncation,
missing/relative/NUL paths and directory refusal. POSIX adds FIFO/device/oversize
refusal. Independent Python JSON checks cover fields, duplicate/obs-text header
roundtrips, schema/type/length/header/budget refusals and upstream MIME results for
HTML, ESM, CSS, WASM, JSON and unknown extensions. HEAD also crosses the actual
native process boundary, with its advertised file length and zero body bytes.
Only local macOS execution is accepted so far; native Linux/Windows CI
must supply their own results.

Still required before Code-OSS resources work: versioned request/body correlation,
trusted helper/document admission, bounded concurrent request lifecycle and
cancellation on navigation/crash, application handler dispatch, application of MIME/
headers in renderer fetch/navigation, sessions and the public Electron
protocol APIs. This component does not enable `file:` access from a custom origin
or change existing renderer file permissions.

See [ADR 0114](../../../../docs/architecture/0114-file-response-metadata.md) for the
wire and ownership decision.
