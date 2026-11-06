# 0081: Portable Web Stream primitives on LibJS

Status: accepted for a partial runtime slice.

Fetch needs incremental bodies, pressure, byte-buffer ownership and cancellation.
Reuse pinned MIT-licensed web-streams-polyfill 4.5.1 rather than inventing a buffered
ReadableStream facade. Bundle its ESM ponyfill with the existing web bootstrap.
Keep package integrity and complete notices in the generated manifest/distribution.
No Node/V8/LibWeb runtime or Ladybird source changes are introduced.

Install stream constructors alongside cancellation globals before application code.
Return separate private abort and stream objects from bootstrap. ModuleLoader roots
both; node:stream/web uses its original constructor snapshot for CJS/ESM regardless
of later global replacement. The module exposes only the 13 implemented stream/
reader/writer/controller/queue classes, not private cancellation hooks.

LibJS supplies ArrayBuffer.prototype.transfer: byte enqueue/BYOB reads detach input
buffers instead of copying and pretending ownership moved. Test transfer, offsets
and independent byte tee storage on actual Mite, alongside Node 24.15.0.

The non-GUI suite also checks locks, queue pressure, writer.ready, serial sink
writes, iteration cleanup, pipe errors/signals/prevention, tee cancellation reasons
and transform errors. A final completion marker prevents unsettled promises from
making a process with no native work appear to pass. Verify CommonJS lookup after
global replacement independently of prior ESM imports.

This package uses public abort notifications; it does not inherit native consumers'
private abort guarantees. Preserve that limitation in the API contract. Complete
WPT coverage, codec/compression streams, Node-stream bridges, transfer between
workers/processes and fetch/body objects remain separate work. Compile Firefly but
keep local GUI tests infrequent and batched.
