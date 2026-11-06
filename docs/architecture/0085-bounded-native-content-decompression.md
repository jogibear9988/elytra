# 0085: Bounded native content decompression

Status: accepted for a partial decode slice.

Pin standalone zlib 1.3.2#2 to the same version already used by the renderer. Reuse
the provider directly through an owned platform/compression adapter. Ladybird's
blocking Stream wrapper assumes input EOF and does not model an asynchronous HTTP
producer's temporary lack of bytes. Keep Ladybird/provider sources unmodified;
do not add LibWeb or a second event loop to Mite. Existing package notice copying
includes the installed native zlib copyright.

Native inflater contexts use shared ownership/RAII and expose only private step/
close functions. Copy input snapshots, consume at most one MiB per call, return at
most 64 KiB and clear zlib pointers before allocating JS result objects. Explicit
GC roots protect factory/handle/results. Keep consumed-byte counts so JS owns the
remaining input rather than the provider retaining JS buffers.

An authored ESM pair coordinates Web readable demand with writable completion,
validates BufferSource input, handles BYOB EOF and releases native state on all
terminal paths. Do not enqueue an entire expansion inside TransformStream's single
transform callback. Store output length before enqueue transfers its ArrayBuffer.
Observe writable abort signals privately but let the abort algorithm own writable
state; erroring the controller during its signal dispatch wrongly rejected abort
and was caught by the differential test.

Expose only gzip/deflate/raw DecompressionStream publicly. Private HTTP modes add
raw-deflate sniffing and gzip padding; neither is a public format escape. Fetch
reverses coding stacks, keeps wire headers, skips explicit empty/null bodies and
preserves cancellation through decoder pipelines. Unsupported algorithms reject;
strict trailer integrity and stronger output pressure are deliberate Node
differences. There is no total decoded-size quota or worker-pool compatibility.

Independent Python data/HTTP peers validate expansion, fragmentation, stacked/
concatenated streams, early flush delivery, corruption/truncation and observed
native closure. Non-GUI embedded-host GC and both-runtime ESM sample exercise the
native resource boundary. Encoders, Brotli/zstd, node:zlib, complete standards/
binding/provider/race coverage and fresh platform acceptance remain open. See
[the decompression contract](../api/decompression.md). Local GUI tests stay batched.
