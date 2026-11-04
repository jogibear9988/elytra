# ADR 0004: native module records and Node resolution

Status: accepted for ESM/JSON file loading; CommonJS refinements are in ADR 0005.

Use LibJS `SourceTextModule`, `Module::load_requested_modules`, linking and
promise-returning evaluation. Code is not transformed to emulate ESM. Install the
VM's host module-load hook for both static imports and dynamic import, including
imports from scripts carrying GC-owned source metadata (ADR 0005). Cache GC-rooted records per VM by canonical
file URL. Insert a parsed record before graph loading so cyclic imports share it.

Resolve URLs using LibURL's C++ API over its existing Rust implementation. File
paths and URLs are separate representations. Validate encoded separators and null
bytes, reject directory imports, canonicalize symlinks, and retain query/fragment
in the identity. Escape filesystem URL delimiters before URL parsing so a literal
`%`, `#` or `?` in a path does not become an escape, fragment or query.

Select `.mjs` or the nearest package.json `type: module`; `.cjs` overrides the
package type. CommonJS file loading is covered in ADR 0005. Package exports/imports
are covered by ADR 0006; ESM syntax detection remains required.

JSON imports use synthetic records; ADR 0005 adds shared CommonJS caching for
plain file URLs while query/fragment copies reuse `JS::parse_json_module`. Validate type=json
before cache access, require it for JSON and reject it for JavaScript. Unknown
attribute names can be rejected by LibJS before our host hook; those TypeErrors
currently lack Node's error code. This is a tracked compatibility limitation.
Unknown URL schemes/formats fail explicitly. Remote file hosts/Windows UNC module
URLs are not yet supported. Native Windows and Linux execution remain unverified.

Root the entry evaluation promise across native event-loop dispatch. Loading and
linking errors propagate as rejections through a native promise continuation.
Timers can settle top-level await. If the loop becomes idle while entry evaluation
is pending, exit 13 with a diagnostic, rather than reporting success.

LibJS's optimized settled-promise await path marks promises handled directly.
Before reporting an unhandled rejection, consult `Promise::is_handled()` as well
as reject/handle host notifications. The fixture suite exercises this path both
in async functions and in handled dynamic import failures.

The reduced build adds LibURL/LibRegex and their dependencies, still excluding
LibWeb and Qt. Upstream builds the Rust static libraries independently with
different feature sets, which currently produces duplicate exported-symbol
warnings on Apple's linker for calendar/panic initialization helpers. The link
and runtime tests pass; do not hide the warning or infer Windows/Linux success.
A unified engine archive or upstream build improvement should remove this before
binary distribution.

References: [Node ESM](https://nodejs.org/api/esm.html),
[Node packages](https://nodejs.org/api/packages.html), and pinned upstream
`Libraries/LibJS/{Module,SourceTextModule,Runtime/Promise}.h` and
`Libraries/LibJS/Rust/src/runtime/async_function_driver_wrapper.rs`.
