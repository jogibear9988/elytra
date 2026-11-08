# CommonJS namespace discovery and evaluation

Status: implemented, locally tested on macOS. This supersedes the named-export
limitation in [the original CommonJS decision](0005-commonjs.md). It does not add
synchronous require(ESM), load hooks or native addons.

## Decision

Use pinned cjs-module-lexer 2.1.1's synchronous pure-JavaScript lexer for static
export/reexport discovery. Run it privately on LibJS, with native C++ owning module
resolution, metadata caches, cycle handling and evaluation. This reuses an existing
source-aware parser rather than maintaining a regular-expression scanner or
coupling compatibility policy to Ladybird parser internals. Mite gains neither
LibWeb nor a Node/V8/Wasm runtime dependency.

Bundle the unchanged dependency through the existing reproducible portable-provider
graph, preserving source hashes, registry integrity and MIT notices. Apply one
exact, guarded in-memory getter-filter adaptation: Node24.15's native lexer retains
a name already found through a recognized assignment even if an unrecognized
getter appears later. Provider2.3.0 has the same older exclusion as 2.1.1; upgrading
alone does not match the measured baseline. Twenty-six public Node/Mite cases
validate the adaptation, literal patterns, getters and unusual source names.

## Ownership and timing

Static linking discovers names without evaluating CommonJS bodies. Reexport walks
use the existing CommonJS resolver, skip builtins/JSON/ESM and publish direct names
before recursion so cycles terminate. Unresolved requests add no names. Bound the
analysis depth explicitly. Metadata stores only native strings; the private lexer
is rooted per loader/realm and never exported as an application builtin.

ESM evaluation enters the authenticated internal loader bridge and executes/reuses
the genuine CommonJS module. Namespace slots receive snapshots of recognized own
properties. Getter exceptions leave undefined; own-property trap errors propagate.
Root the export value and every already captured slot while later getters can run
arbitrary application code and collection. Default and module.exports markers retain
the exact whole export value. Quote every generated export name as UTF-16 source
data, never as a generated identifier.

Keep the original successful CommonJS export in private realm-owned storage;
replacing require.cache[file].exports subsequently affects require but does not
retarget the first namespace. A real deletion/reload refreshes this original value.
Source edits and arbitrary hook changes after cached analysis remain unaccepted.

## Acceptance and limits

Seven public differential workflows cover lexical detection, snapshots, chains,
cycles, original-export retention, reload and missing-export failure before native
file side effects. The separate classification gate exercises the generated parser
through real module imports. The unchanged compiled Code-OSS ZIP consumer now uses
real yazl and native file streams; independent archive CRC and exact extracted-byte
checks pass. No extension installation, workbench, screen capture, full Node loader
parity or fresh native Linux/Windows acceptance follows from these gates.

References: [upstream lexer](https://github.com/nodejs/cjs-module-lexer),
[Node24 translator](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/modules/esm/translators.js),
[API contract](../api/module.md#commonjs-named-exports-in-es-modules).
