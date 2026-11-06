# 0084: Immutable Blob/File and multipart FormData bodies

Status: accepted for a partial native-runtime data slice.

Reuse the MIT fetch-blob 3.2.0 immutable byte core in a portable data bundle. Version
4's async Node fallback does not fit synchronous realm bootstrap. Replace 3.2's
optional legacy streams import through an owned build resolver, not package edits.
Capture native codecs/Buffer via a bundle injector, and supply Web Streams from the
private host. Include original package notices/integrities with canonical LF license endings. Assert that only four
intended packages reach the data bundle; no second stream polyfill/DOMException
shim or Node/V8/LibWeb runtime is shipped.

Owned Blob/File wrappers normalize MIME/endings/scalar strings, expose modern bytes
and route internal provider slices through the same bounded BYOB/cancel adapter.
FormData owns ordered branded string/File entries. Export original Blob/File globals
also from node:buffer. The native loader roots data helpers through private Fetch
host state. Replaceable public globals do not control later provider reads.

Encode FormData once into immutable multipart Blob parts, with native CSPRNG
boundaries and safe header/newline encoding. Store immutable body sources so
redirect replay uses exactly the same body/boundary; no whole-file concatenation
is required for upload. Record the actual Node 24.15 changed-boundary redirect
defect separately. A Python MIME parser verifies wire lengths, order and file hashes.

Convenience blob/formData consumers collect bytes explicitly. Reuse pinned busboy,
streamsearch and WHATWG MIME parsing for receive-side decoding. Disable silent
field/file truncation, preserve path/UTF-8 filenames and entry order, decode tested
base64 File content, retain parser header validity/size checks and report malformed
data as TypeError. Four validated Buffer codec slices satisfy parser dependencies.
Load the parser lazily rather than initialize Node streams for Blob-only programs.

Tests cover object ownership, codec/stream snapshots, BYOB EOF/cancel, 1.1-MB fields,
262145-byte binary upload, redirect replay, parsed network responses and modern ESM
sample behavior under Node/Mite. Exact WebIDL, unusual charset/disposition/transfer
encodings, every GC/abort/large-buffer boundary, filesystem-backed files, Blob URLs,
renderer IPC transfer and full WPT remain open. Preserve Ladybird and keep local
GUI testing infrequent/batched. See [the data API contract](../api/blob-formdata.md).
