# ADR 0116: Native file frames into LibWeb responses and body streams

Status: accepted for the local macOS LibWeb component. Real scheme dispatch,
helper/document binding and native Linux/Windows execution remain separate gates.

## Decision

Adapt the existing file-response frames to actual LibWeb PendingResponse,
FetchController, FetchBodyDeliveryGate and FetchByteChannel objects. Keep C++ at
this existing C++ engine boundary, with explicit GC roots for pending/controller
state. Do not introduce a parallel JS stream or buffer complete files in the
renderer. No page/native setter or public protocol export is added.

PendingResponse has no exported upstream C++ interface. Compile the owned adapter
inside LibWeb and export our own native class, including a callback-forwarding
method. Do not patch upstream visibility or duplicate that implementation. Link
only the pure metadata/frame codec into LibWeb; checked files and request owners
remain in the main-side library. Shared budgets live in a small Limits header.
Standalone Mite stays outside this graph and Ladybird remains unchanged.

The native caller supplies admitted epoch/ID, live FetchParams and transport
credit/cancel callbacks, then roots the returned pending response before delivery.
Reject cancelled admission and an empty native fetch task destination. Frame and
metadata identity, HEAD, lengths, order and credit must agree before delivery.
Resolve the actual native response at headers, preserving its status and fields.
A stream body uses upstream's channel with 32/64-KiB low/high watermarks. HEAD has
no body or credit. Completion checks exact length and closes the channel normally.

The gate reports remaining allowance, not additive grants. Send only newly granted
credit and account for bytes before writing. A weak transport proxy avoids a
native owner/channel/gate cycle; a retained callback owner protects a callback
that reentrantly stops its own response. All adapter entry points remain on the
owner thread.

Install the controller's body transport before any metadata so abort before
headers reaches the owner. Abort, reader cancellation, remote cancellation,
malformed frames, teardown and destruction release native VM references and issue
at most one cancel callback. After headers, transport failure must error the
channel: ConsumerCancelled alone does not settle an upstream stream reader.
Before headers resolve an appropriate aborted/generic network error. Detach the
gate and clear pending/controller/params/proxy ownership on terminal paths.

## Evidence and limits

The non-GUI native test uses a real principal realm/document, FetchParams and
controller with an actual task destination. A real checked 4 MiB + 19 byte file
passes through FileRequests, encoded frames, PendingResponse, FetchByteChannel
and the actual upstream ReadableStream reader. Validate each byte, response status,
MIME/length/cache headers, credit, completion and GC between chunks.

Also exercise actual controller abort before metadata and with queued body bytes,
reader cancellation, HEAD, empty completion, reentrant credit cancellation,
producer over-credit, wrong epoch/ID/offset, missing/duplicate/malformed metadata
and incomplete completion. Errors must settle real pending responses/readers;
a source-only guard is not acceptance. Require this component in macOS release/
check-firefly and experimental native Linux/Windows renderer workflows.

This is engine response adaptation, not a successful URL load or page fetch.
The native test does not execute the public JavaScript fetch dispatcher, create a
desktop window or bind a live WebContent channel. Next connect scheme fetch to the
real admitted helper/document request service, enforce receiver request quotas and
revocation/deadline wiring, and implement application protocol/session dispatch.
Navigation transfer, worker policy and CORS/CSP/cookie/COOP/COEP response behavior
need real renderer acceptance. Public protocol and Code-OSS workbench remain open.
