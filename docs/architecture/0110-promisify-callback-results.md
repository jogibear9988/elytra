# 0110: Callback Promise adaptation and builtin result metadata

Implement util.promisify in maintained ESM JavaScript. Receiver forwarding,
exception/settlement behavior, descriptors and custom symbols are engine-independent
and need no native service or extra FFI. Preserve callback ownership of completion
and the native event-loop/microtask ordering. Node is only the test/build oracle.

Generic conversion handles the first success argument. Existing filesystem read/
write and DNS lookup callbacks need named multiple results. Share a private symbol
through a GC-rooted per-loader state object supplied only to the owned factories.
Functions receive immutable hidden metadata under that symbol, and promisify
snapshots the argument names when constructing a wrapper. No public registration
API or mutable public util facade lookup is involved. Repeated conversions still
create distinct wrappers; do not use the public custom symbol as a shortcut for
these ordinary named-result callbacks.

Use the public globally registered custom symbol for genuine custom implementations:
execFile retains stdout/stderr and the immediately available child on its Promise;
setTimeout/setImmediate return their existing Promise timer implementations with
private abort subscriptions and ref/unref semantics. Native service ownership and
raw-FS facade separation remain unchanged. The shared metadata object/symbol is
realm-local; no process-global registry or renderer privilege is introduced.

Node 24.15/Mite comparisons cover identities, receiver, descriptor/accessor/symbol
preservation, falsy errors/custom values, exact exceptions, callback settlement,
thenable adoption, custom accessor order and pending callback/receiver GC. Actual
filesystem/descriptor I/O, numeric DNS, child output/failure and native timer
cancellation validate builtin adaptations. Existing unsupported service/options
continue to reject. DEP0174 emission for Promise-returning originals remains a
documented difference; this is not full node:util support.

The actual full-source Code-OSS Firefly probe now passes the promisify import and
fails at the absent Electron protocol export, retaining exit 1. No replacement
mainImpl or protocol placeholder is introduced, and no workbench window is opened.
The generic service tests do not prove execution of Code-OSS's later filesystem
workflows. See the [API contract](../api/util.md) and
[priority inventory](../compatibility/vscode.md). Ladybird remains unmodified.
