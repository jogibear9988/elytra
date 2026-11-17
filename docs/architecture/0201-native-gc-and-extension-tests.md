# ADR 0201: Native GC opt-in and regular extension-test startup

Status: implemented and locally tested on macOS ARM64. Full Code-OSS editing and
fresh platform execution remain unaccepted.

## Demand

The regular Code-OSS development-extension/test path was blocked by an owned
engine-default adaptation and its `--expose-gc` request. Silently discarding that
flag would misrepresent an observable native operation. General inspector/V8
options cannot be claimed by LibJS. The goal is the original extension-test
lifecycle and real editor APIs, with no test-specific fake modules.

## Decision

Implement exact `--expose-gc` in Mite's native CLI. Separate an explicit native
OwnerSession capability from reported execArgv metadata. Install `gc` before
embedding initialization, and call the owning LibJS heap's ordinary collector,
never its all-objects destruction mode. Default owners expose no gc function.
Support synchronous gc()/gc(undefined), preserve live roots and current-job weak
retention, and explicitly refuse option bags, asynchronous/generational modes or
extra arguments. This uses C++ beside existing VM ownership, not Node or V8.

Scan CLI options through eval/print source until the entry/application boundary
or `--`, retaining actual execArgv order and duplicates. Optional print source
must read real piped bytes rather than invent an empty script. Fail on read
errors or an unsupported interactive REPL. Only the native Windows CLI selects
binary standard streams; embedding callers keep their process stream policy.

Firefly's utility facade snapshots an empty or single exact GC flag; native spawn
revalidates independently before creating an endpoint. Place the flag after the
owned utility mode and before the entry file. Bootstrap strips only that mode and
preserves Mite's genuine execArgv. Default utility children do not inherit parent
engine flags. No renderer GC API is introduced.

Extend the existing pinned Code-OSS adaptation's captured input with original
structured development locations. Admit only paired local file URI/path modes;
recheck private mode/path provenance after asynchronous port/environment setup.
Validate the exact original execArgv sequence and retain --expose-gc only in test
mode. Continue to omit and disclose implicit inspector/DNS/network tuning, and
refuse explicit debugging/profiling/V8 options. Do not alter the original
extension host, test runner, initialization barriers or application shutdown.

## Evidence and remaining work

Thirty actual Node/Mite controls cover CLI flags, ordering, boundaries, stdin,
root/weak GC and unsupported options. Real utility children cover opt-in/default
GC, precise execArgv/appargs, connected port snapshots/FIFO, process lifecycle and
parent teardown. Forty policy cases in each runtime cover paired URIs, malformed
or changed provenance, exact flag arrays, raw unsupported switches and untouched
Node/Electron branches. Source adaptation reversal and missing/duplicate/drift
checks remain strict; Ladybird is unmodified.

The refreshed original Code-OSS run reaches the extension-test loader after its
eager-activation barrier, then fails on the genuine vscode ESM hook's data URL.
This proves progress through the prior startup restriction, not a working test
extension or editor workflow. Implement standard data-URL module loading next;
CommonJS hook integration, remaining native services and broader platform/editor
acceptance stay open. No debugger availability, all-extension success or V8 GC
option parity is claimed.
