# Preserve Node legacy URL semantics separately from WHATWG URLs

## Decision

Port the pinned Node 24.15.0 legacy parse/format/parseHost algorithms into maintained
ESM, keeping full MIT notices and immutable upstream source/license hashes in
vendor/node-url. Run the JavaScript algorithm on LibJS. Do not implement url.parse
by calling new URL: default ports, numeric hosts, relative paths, escapes, auth and
query fields have observable legacy semantics required by Code-OSS's nodeRequest.

Keep existing WHATWG URL behavior unchanged. Add a separate native ICU UTS #46
domain conversion operation for legacy parsing, excluding WHATWG IPv4 normalization.
Preserve Node's non-strict DNS-length/hyphen/empty-label policy while rejecting
contextual, bidi and disallowed input. Native ICU/profile differences remain subject
to differential acceptance; a source-derived parser alone is not complete parity.

## Query and error boundaries

Use Node-derived legacy query encoding/decoding, duplicate arrays, null-prototype
objects and the 1000-pair limit. Valid/unescaped query paths preserve raw UTF-16;
malformed percent/UTF-8 fallback must reproduce Node's byte handling. The first
implementation reused form decoding too broadly; targeted lone-surrogate tests
found real differences, now covered alongside actual integer/Unicode/native IDNA
cases. Do not mistake URLSearchParams scalar normalization for querystring parity.

Expose parse and Url, with prototype parse/format/parseHost. Resolve APIs remain
explicitly unsupported; module-level format is not added by this slice. Node legacy
deprecation warnings are not emitted. New code should use WHATWG URL, and all URL
consumers must validate their own access policy: legacy parsing is no security boundary.

See the [API](../api/url-legacy.md) and [progress](../PROGRESS.md) for actual Node/Mite
fixture counts, pinned source hashes and unverified platform scope.
