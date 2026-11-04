# 0016: Portable stream sources on Mite's host primitives

Status: accepted for a partial stream implementation; native macOS ARM64 tested.

## Decision

Reuse readable-stream 4.7.0 and string_decoder 1.3.0 as pinned, bundled JavaScript
sources running in LibJS. They provide established incremental decoding and stream
state machines without coupling Mite to LibWeb, Node or V8. Integration adapters,
source generation and builtin registration belong to our repository. Ladybird
remains unmodified. This adds no new Rust/C++ boundary.

An exact npm lockfile and esbuild 0.25.12 produce checked-in factory expressions.
A private host supplies builtin imports, retained native platform information and
a validated byte-view helper. Imports resolve to Mite's Buffer, EventEmitter,
process.nextTick and StringDecoder; npm copies of buffer/events/process are not
bundled. stream/promises shares the stream.promises object. The default stream
export is the base Stream constructor. Existing fixed ESM named-export shims apply.

The portable library's byte high-water mark is adapted through its public setter
to Node 24's default (64 KiB except 16 KiB on Windows; objects remain 16).
StringDecoder's own wrapper normalizes labels, adds base64url, preserves string
passthrough and resets pending state after end, including repeated end calls.
The native view helper reads engine buffer slots rather than user-overridden
byteLength/byteOffset/buffer properties, then supplies the portable decoder with
an actual Buffer view. Detached/out-of-bounds inputs follow the existing empty
view policy. The portable decoder implementation itself remains unmodified.

## Validation and limits

Seven workflows compare exact output with Node v24.15.0: readable buffering and
UTF-8 chunks, writable backpressure/drain, cork/writev batching, Promise pipeline
through async generators/transforms, isolated error cleanup, async iteration and
early-return behavior, and builtin/class/default-watermark identity. StringDecoder
has 449 shared comparisons for incremental splits, malformed/truncated bytes,
aliases, base64url, repeated flush, strings, views/shared/detached storage and
spoofed view metadata.

A separate concurrent-error fixture deliberately retains distinct goldens for
Node and Mite: a pipeline's cleanup callback and an independent null-value stream
error arrive in a different relative order. Running this same portable bundle in
Node with Node's native scheduler also reproduced Mite's order. This is a portable
library revision difference, not evidence of full Node stream scheduling parity.
Both outputs remain visible and independently asserted; this case is not counted
as a matching differential workflow.

Other package exports (including compose, finished, addAbortSignal and stream
inspection helpers) are available but have not received broad differential
coverage. WHATWG stream adapters, global AbortController, stdio/filesystem streams
and Blob are not provided by this step. The bundled AbortController shim is private
to the library, not a global API. StringDecoder's internal fields/descriptors,
invalid receivers and exhaustive error messages are unverified. Native Windows
and Linux remain unverified. This does not claim the full upstream package test
suite or complete Node stream compatibility.

## Reproducibility and licensing

`tooling/runtime-js/README.md` describes updates. The generated manifest records
package versions/integrities, all contributing source hashes, our entry hashes,
lock/generator hashes and license hashes. Full notices accompany and are embedded
in the factories. Every Mite build and a CTest gate check checked-in artifacts and
adapter inputs with Python, without requiring Node/npm to build the executable.
An explicit generator --check additionally verifies exact regeneration with the
locked build dependencies installed. Hash checks establish consistency, not a
security audit of third-party code. Review dependency updates before regeneration.
