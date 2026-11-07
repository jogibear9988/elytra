# ADR 0099: Bounded native binary snapshots and wire attachments

Status: implemented internal ArrayBuffer slice; local macOS ARM64 native wire and
separate-process acceptance pass. Public IPC/contextBridge still reject binary
values. Native Linux/Windows execution of this new slice remains pending.

## Context

The prioritized Code-OSS target needs binary messages and MessagePorts. Existing
Firefly wire graphs encode plain data as bounded JSON. The public native route
also passes them through BiDi JSON and main-process JavaScript strings. Arbitrary
bytes cannot safely use that route; base64/JSON number arrays would add avoidable
encoding and allocation costs. A codec alone does not establish public transport.

## Decision

Add a strictly opt-in binary budget to BridgeData::Limits, default zero. Existing
public capture/encoding/decoding policies remain unchanged. The first internal
slice accepts exact native ArrayBuffer objects from the selected source realm,
with fixed, attached, nonshared storage. Copy the bytes into owned native storage;
retain no VM pointers. Ignore custom prototypes and expando properties without
reading their getters/hooks. Normal graph edges preserve repeated buffer identity
and cycles. Resizable/shared/detached buffers, proxies, views and host objects are
not admitted by this slice.

Materialize into an explicit mutable destination with a new destination-realm
ArrayBuffer and independently owned bytes. Reject binary graphs when callers
request frozen publication: Object.freeze(ArrayBuffer) would not freeze its backing
bytes. Do not silently weaken contextBridge's frozen plain-data contract.

## Private wire format

Plain-data graphs retain version 1. Binary graphs use an eight-byte header:
ASCII FFB2 followed by the little-endian uint32 metadata length. Metadata is the
bounded JSON graph envelope [2,nodes]. ArrayBuffer nodes use tag 8 and their byte
length. Append each buffer's exact raw bytes in node order after the metadata;
there is no base64 or JSON byte array. NUL, SUB, CR/LF and non-UTF-8 bytes remain
unchanged. The format is internal, not an application/network protocol promise.

Require an explicit nonzero binary budget on both encode and decode, including
empty buffers. The hard binary budget is at most one MiB and the complete frame
still fits the existing one-MiB internal wire bound. Tests opt into 65,536 bytes;
the public IPC wire limit remains 128 KiB and does not enable this format.

Validate header/metadata length and aggregate payload bounds before parsing or
copying attachments. Validate the existing graph budgets, node arity/types,
references, reachability and duplicate keys. Refuse function tokens, unknown tags,
truncated/overlong attachments, unclaimed bytes and binary frames without buffers.
Budget and schema failures remain errors, not partial materialization.

## Acceptance

Native same-VM/separate-realm tests verify exact 65,536-byte data, repeated-buffer
identity, cycles, empty buffers, hostile expando/prototype hooks, source mutation
and GC, destination mutation isolation, default/frozen refusal, exact smaller-budget
refusal, detached/resizable/view/proxy refusal and malformed remote frames.

A real encoder process exits before a fresh decoder starts. An independent Python
peer verifies the full raw payload and byte count and constructs its own frame,
which must decode and re-encode canonically in another VM/process. Preserve native
Windows stdin/stdout binary mode for these tests; text mode would corrupt payload
bytes. The existing plain-data wire regression suite remains required.

## Remaining work

This is copied storage, not zero-copy or ownership transfer. Uint8Array/Buffer and
view/backing-store alias semantics need a separate implementation. Connect binary
frames through a native byte-capable transport rather than existing BiDi/JS text
fields, define publication semantics and test actual page/preload/main traffic.
Then implement event/MessagePort APIs, bounded streaming and authorized lazy file
access. Require native platform, navigation/revocation/pressure/crash and independent
Electron comparison gates before enabling public support or performance claims.

See [Code-OSS target](../compatibility/vscode.md) and
[file transport plan](../guides/file-transfer.md). Ladybird remains unmodified.
