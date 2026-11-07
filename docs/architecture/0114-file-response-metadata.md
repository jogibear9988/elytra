# ADR 0114: Bounded native file-response heads

Status: accepted for the local macOS native component. Renderer integration and
native Linux/Windows execution remain separate acceptance requirements.

## Context

The pinned Code-OSS protocolMainService uses a file protocol callback returning an
authorized path and HTTP headers, including COOP/COEP, cache and Document-Policy.
[FileBody](../../firefly/native/protocol/files/README.md) already reads checked
regular files and streams bounded raw bytes to a native child. That does not tell
a consumer the MIME type, declared length, HEAD semantics or response headers.

## Decision

Keep response metadata in C++, reusing existing AK JSON, Core MIME lookup and HTTP
field validation. This sits beside an existing C++ file owner and byte transport;
another Rust FFI would not improve this boundary. Add no parser copy, upstream
changes, Node/V8 dependency or Mite-to-Firefly dependency.

A version-1 file-response head contains exactly schema, kind, nonzero 32-bit
request ID, fixed status 200, length up to the FileBody limit, typed HEAD flag and
ordered string-pair headers. This head is at most 32 KiB and 64 header pairs.
Raw file body chunks remain outside JSON. Neither source paths nor native
handles are implicitly encoded. The request ID is an identifier, not authority;
trusted endpoint/document/request admission still needs the request service.

Generate Content-Length from the opened file and infer Content-Type using
upstream's filename MIME table unless supplied. Reject caller Content-Length
and duplicate Content-Type/Length. Preserve other header order and duplicates.
Repeat HTTP field, count, raw-byte and encoded-byte validation in the codec.
Reject connection/transfer framing and hop-by-hop fields for this exact-length
file representation. Unsupported statuses, ranges, redirects and errors need
explicit future service contracts; this is not a generic HTTP response API.

HTTP header values are byte sequences, not UTF-8 strings. Map each byte to the
same Unicode code point for JSON and explicitly recover bytes on decode. This
preserves obs-text without accidental UTF-8 expansion in the HTTP header value.
Non-byte characters and invalid HTTP names/values fail. The JSON parser's existing
nesting bound remains in force and the entire input is budgeted before parsing.

## Evidence and limits

The real native child admits the response head before body bytes, checks preserved
COOP/COEP/cache/Document-Policy fields, duplicate cookies and obs-text bytes, then
validates every byte of a 4 MiB + 19 byte transfer and the declared body length.
HEAD crosses the same native process boundary with its full file length and zero
body bytes. The child receives no file path. Empty/body mutation/cancellation tests
remain required by the same component gate.

An independent Python JSON reader/writer checks schema, identity/length boundaries,
HEAD, duplicate/byte-preserving headers and MIME inference for HTML, ESM, CSS, WASM,
JSON and unknown suffixes. Negative controls include unknown fields/status/schema,
wrong types, malformed pairs, framing headers, missing/duplicate/mismatched length
and type, count/raw/escaped budgets, non-byte values, NUL/DOS-EOF tails and excessive
JSON nesting. The native builder is checked separately from its decoder. Native
fixture standard streams are binary on Windows; Python explicitly uses UTF-8.

This proves metadata and raw-file delivery to an owned native test consumer. It
does not apply response headers or their security effects in LibWeb. CORS/CSP,
cookie filtering/storage, COOP/COEP/isolation, real fetch/navigation, helper/document
revocation, bounded concurrent requests, request/body correlation, sessions and
public application handler dispatch remain required. No Code-OSS workbench or
public Electron protocol support follows from this component result.
