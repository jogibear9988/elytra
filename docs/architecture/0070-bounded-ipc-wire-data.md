# 0070: Bounded data graphs for main/renderer IPC

Status: native codec and separate-process tests implemented on macOS ARM64.
Application IPC routing, sender authentication and public invoke/handle remain
unfinished. This is an internal building block, not an Electron IPC API claim.

## Decision

Extend BridgeData with encode_wire/decode_wire instead of serializing live JS values
through JSON.stringify. Capture retains its existing plain-data ownership/prototype
checks and refuses getters, proxies, host objects and unsupported capabilities.
Wire encoding also rejects the internal function tokens used by context publication.
The codec stores native strings/numbers/indices only, with no GC or VM pointers.
Decoding validates the entire graph before materialize allocates any JS objects.
No Node/V8 runtime, upstream source modification or new dependency is introduced.

Use a private versioned ASCII JSON array graph: [1, nodes], with root index 0. Each
node is a fixed-arity tuple. Object/array edges refer to node indices, preserving
cycles, aliases, holes and undefined without recursively nesting the wire JSON.
All node indices must be in range, all nodes reachable from root, and property keys
unique per container. No JSON object occurs in the wire format, avoiding duplicate
schema-key ambiguity and prototype interpretation by JSON deserializers.

| Tag | Node tuple |
| --- | --- |
| 0 | [0] for undefined |
| 1 | [1] for null |
| 2 | [2, boolean] |
| 3 | [3, sixteen lowercase hex digits of an IEEE-754 binary64 bit pattern] |
| 4 | [4, UTF-16 code-unit array] |
| 5 | [5, null-prototype boolean, property tuples] |
| 6 | [6, array length, property tuples] |

A property tuple is [UTF-16 key units, value-node index, enumerable boolean]. UTF-16
units are integers 0..65535, preserving unpaired surrogates and NUL in keys/values.
Numbers use bit patterns instead of decimal JSON conversion, retaining finite
precision, subnormals, negative zero and infinities across host endianness. Encode
NaN as 7ff8000000000000; reject other NaN encodings before passing them to LibJS's
NaN-boxed value representation. Require an IEEE binary64 host at compile time.
Numeric metadata must have integral finite values within its explicit bound;
booleans and truncated fractional indices/units are not integers.

Arrays cannot supply an own length property or an array index at/above their
stated length. Ordinary non-index names retain ordinary JS property semantics.
Materialization defines own data properties directly, so __proto__ is inert data.
Receivers choose frozen or mutable reconstruction in trusted C++ code; the wire
cannot choose it. The default remains frozen for all existing contextBridge calls.
Mutable reconstruction supports future IPC clone semantics without sharing source
objects or changing existing API publication behavior.

## Resource bounds and errors

Before JSON parsing, reject empty or over-1-MiB input, object syntax and array
nesting beyond 8, with a string/escape-aware scan. The full schema then rejects
unknown versions/tags, wrong arity/types, duplicate keys, invalid numbers/UTF-16/
references, orphan nodes and invalid sparse-array metadata. Parser and graph failures
return ErrorOr errors, never a partially materialized object or evaluation fallback.
Whitespace/ordinary numeric metadata spelling can normalize on re-encoding; this is
not an authenticated canonical-message protocol.

Hard maximums are 4096 nodes, 4096 own properties including implicit array length,
65536 cumulative UTF-16 units including keys/implicit length, first-visit graph
traversal depth 64, per-array length 4096 and aggregate array length 16384. The last
bound prevents many sparse arrays from multiplying output storage despite a small
wire input. Callers may lower the usual BridgeData limits but cannot widen them.
Both encode and decode enforce those graph bounds and the wire byte cap.

The JSON parser still creates a temporary native tree bounded by the input size;
this is not a claim that memory usage equals wire bytes or that arbitrary source
object enumeration has hard scratch-space containment. Existing capture behavior
and that limitation remain recorded in ADR 0028. IPC still needs independent
pending-work/queue/time budgets and document lifetime policy.

## Evidence and integration still required

The dedicated native wire checks compare decoded values in another realm, with frozen and
mutable ownership, GC, cycles/aliases/holes, nonenumerable keys, inert __proto__,
UTF-16/NUL, finite extremes and IEEE special values. They exercise malformed schema,
indices/fractional fields, duplicate keys, invalid NaNs and internal function tokens,
byte mutations, depth/node/property/string/array limits, exact node/UTF-16/sparse-slot
boundaries, and recovery after rejection/collection.

The engine permits only one VM per process. An attempted second VM in the SDK
correctly hit that upstream guard; retain it and test the actual boundary instead.
The same native test executable also runs as a producer: it serializes a real LibJS graph and exits. A fresh process
creates its own VM, decodes/materializes, verifies fresh intrinsic prototypes and
identity/precision semantics after GC, then captures/re-encodes exactly the producer's
canonical bytes. The heavy malformed-input/1-MiB checks run in their own process,
so their allocator/stack residue does not contaminate the browser SDK's conservative
GC fixtures. An integrated run exposed a StackPointer -> RawNativeFunction -> Realm
retaining path; no browser GC assertion or runtime policy was relaxed. The unchanged
browser SDK also passes ten predetermined fresh-process runs.
Bad remote inputs must exit 1 with no output, not abort or hang.
A subsequent valid process transfer still passes. check-firefly-isolation depends
on this check-firefly-wire gate, so normal release tests include both.

No wire reader is exposed to application pages, no new public module export is
installed, and no IPC authentication is inferred from a valid data graph. Next
integrate native per-page transport with exact window/frame/document generation
validation, request/reply identity, bounded pending work and teardown/crash rejection.
Then run real Electron/Firefly invoke/handle migration fixtures and malicious/stale/
foreign-sender tests before advertising that public API. Native Windows/Linux
renderer and packaged-wire acceptance remain separate work.
