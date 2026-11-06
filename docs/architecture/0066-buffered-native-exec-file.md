# 0066: Buffered native execFile over the existing process transport

Status: implemented; local acceptance recorded in PROGRESS, hosted acceptance pending.

## Decision

Add callback-based node:child_process.execFile using the existing native spawn,
pipe streams and EventEmitter lifecycle. Keep orchestration in the owned JS
builtin; add no native process implementation, language boundary or dependency.
Return the live ChildProcess and capture each output stream independently. Preserve
incremental text decoding, Buffer mode, callback overloads, output limits,
timeout/killSignal and native startup errors. Call back once on close, or on the
earlier native startup error; callback exceptions use the existing fatal boundary.

The default output budget is 1 MiB per stream. Overflow closes both readers and
requests termination. The explicit Infinity option is unbounded. Follow observed
Node encoding and decoded-string truncation behavior, including code-unit slicing
with a byte-derived limit; Buffer mode retains an exact byte prefix. Document the
distinction rather than silently promising an exact UTF-8 byte-sized string.
Stop/error/close racing cannot invoke the callback twice. Neither timeouts nor
budget overflow certify descendant-process supervision or immediate termination.

This step does not add shell execution, exec, fork, synchronous APIs, AbortSignal,
privilege changes or util.promisify. Unsupported native modes fail explicitly;
Node-compatible ordinary option validation remains synchronous. The original
spawn API and its stdio/ref/unref policies retain their separate contract.

## Evidence

Compare actual Node/Mite executions of independent Python children for output,
errors, byte limits, decoding, stdin, timeout and lifecycle. Check raw Buffer/text
type as well as hashes/lengths; do not normalize output. Include callback-free
execution, asynchronous/once callback checks, fatal callback propagation, a native
startup failure and differential option validation. In Firefly, require a real
buffered child alongside existing Mite HTTP and renderer operations. Update the
API/matrix and retain all existing process/security gates.

The baseline is [Node 24.15's execFile contract](https://nodejs.org/download/release/v24.15.0/docs/api/child_process.html#child_processexecfilefile-args-options-callback)
and direct behavior, not an embedded Node runtime. Ladybird remains unmodified.
