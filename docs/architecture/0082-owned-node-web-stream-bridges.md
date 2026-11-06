# 0082: Owned Node/Web stream bridges

Status: accepted for a partial interoperability slice.

The pinned portable Node stream package has adapter entry points backed by empty
internal Web Stream hooks. Installing globals alone does not make those methods
work. Supply authored ESM adapters under runtime/bootstrap/streams and override
Readable/Writable/Duplex fromWeb/toWeb when constructing the shared stream module.
Do not patch the dependency or Ladybird.

The native loader supplies private captured Web constructors and cancellation
hooks. Conversions stay incremental: readers feed Node on demand, Web queues
pause/resume Node, Node pressure/drain drives Web sink writes. Buffer chunks become
independent byte snapshots; arbitrary objects retain identity. Duplex reuses the
same fromWeb state machine for two endpoints and the two toWeb directions.

Validate options before acquiring Web locks, and validate both pair endpoints
before either is locked. Node 24.15 can strand a reader on invalid highWaterMark;
preserve this deliberate ownership improvement as an explicit differential test.
Retain acquired fromWeb locks after completion as Node does. Destroy observes both
Web shutdown Promises; Web cancel/abort observes native terminal closure. Private
AbortSignal subscriptions are disposed at destruction and ignore public event
propagation suppression. Terminal failures remove data/drain listeners and retain
an error guard for late legacy notifications. Record Node's retained-listener
counts separately instead of pretending internal listener cleanup is identical.

Use actual Node/Mite comparisons for byte/object mode, encoding, pressure, signal
reasons, errors, early close, Duplex transforms and roundtrips. Independent Python
HTTP barriers require first-block delivery before the server sends the remaining
body, verify streamed upload bytes and observe cancellation closing the connection.
This establishes the transport/body boundary for fetch without claiming fetch is
already implemented. Mixed-pipeline/finished Web integration, all TLS/half-close/
GC races, file streams and transferable streams remain separate coverage/work.

See [the stream API contract](../api/stream.md). Compile Firefly without repeatedly
launching local desktop tests.
