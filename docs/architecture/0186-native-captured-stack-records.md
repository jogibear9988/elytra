# ADR 0186: Native captured stacks with GC-owned lazy records

Status: implementation; acceptance and remaining limits are recorded in PROGRESS.

Code-OSS and npm bindings-style helpers need Error.captureStackTrace and source
CallSites. A copied new Error().stack cannot trim by actual function identity or
preserve lazy headers, arbitrary cached values, formatter hooks and GC ownership.

Capture public LibJS VM.stack_trace metadata on the VM owner thread. Constructor
trimming compares real frame function identity. Copy every borrowed execution
context into owned names/source ranges before creating JS objects or calling
intrinsics; no execution-context pointer outlives capture. Default frame strings
and structured source metadata describe the actual engine rather than inventing
V8 frames.

A private native WeakMap associates each target with a GC-owned record containing
its target, frame strings, CallSites, cache and formatting guards. Native functions
capture direct full GC::Ptr fields; upstream GC::Function visits the raw capture
range. Cached arbitrary values are JS property edges, not untraced C++ values or
process-lifetime roots. An independent private WeakMap brands CallSites and owns
immutable metadata records. Retained sites have no capture-owner edge.

Lazy getters consult the original Error constructor's prepare hook. A realm guard
bypasses nested prepare calls using real default formatting; a record guard rejects
recursive header access explicitly. Scope guards restore both on abrupt completion.
Getter/setter borrowing finds the actual capture holder through ordinary prototype
chains; Proxy paths avoid traps. Frozen targets keep mutable private caches.

Capture limits use the engine's data-only property lookup, since Node ignores
stackTraceLimit accessors. Source methods expose only known metadata. Constructor
classification is deliberately unavailable: actual differential tests disproved
using ExecutionContext.caller_is_construct, which is an interpreter return-control
flag set only on a fast path. General Rust new_target metadata has no equivalent
current C++ accessor. Supporting it needs a genuine owned ABI seam; guessing from
names, this/prototypes or function constructibility is incorrect.

The first default-capture implementation explicitly rejected all custom hooks.
A fresh actual Code-OSS trace then exposed bindings.js using prepareStackTrace for
caller-file discovery and leaving its hook installed when that call threw. Adding
real source CallSites and reentry behavior addresses the actual consumer without
loosening addon, permission or runtime boundaries. Full V8 introspection, async
stack stitching and automatic hooks for ordinary engine errors remain separate.

No Ladybird source is modified and Mite retains no Firefly/LibWeb dependency.
[Node error API](https://nodejs.org/docs/latest-v24.x/api/errors.html) and
[V8 stack trace reference](https://v8.dev/docs/stack-trace-api) guide behavior;
actual pinned Node/Mite/host tests decide the accepted scope.
