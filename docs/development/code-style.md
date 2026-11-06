# Source organization and readability

Source readability is the current implementation priority. Complete this refactor
before resuming new API work. Preserve runtime behavior and the unmodified
Ladybird pin while changing layout, authoring format and comments.

## Organization

The [native runtime map](../../mite/runtime/README.md) separates core VM/event-loop
ownership, module loading, native service families and the CLI. Include headers
through their subsystem paths so dependencies are visible at the use site.

Maintain equivalent boundaries in Firefly: desktop API, native window integration,
renderer worlds/serialization, callback adapters, helper-process entry and packaging.
Build/test scripts and application examples should be easy to locate by purpose.
Do not mix generated bundles or third-party implementation into authored sources.

## JavaScript authoring

Use ESM for maintained implementation sources, build tools and examples. Internal
builtin factories need a generated synchronous bootstrap form because require()
and initial global construction cannot await ESM evaluation. Author that logic as
ESM and generate/check the embedding adapter; application ESM and CommonJS must
continue sharing the same native builtin instances. CommonJS compatibility fixtures
remain CommonJS because that is the behavior they test.

Use normal indentation, descriptive intermediate names and blank lines around
function/method definitions. Expand compound one-line control flow. Prefer small
named helpers where they express a meaningful operation or ownership boundary.

## Comments

Each production source should explain its role and relevant dependencies. Document
native ownership, thread/realm constraints, close/revocation, error translation and
non-obvious compatibility decisions next to the code that enforces them. Public
interfaces need contracts; trivial getters do not need prose repeating their name.
Keep comments accurate when moving or changing code.

## Formatting and validation

[Style tooling](../../tooling/code-style/README.md) pins the JS, native and Python
formatters. The repository EditorConfig provides matching editor defaults.
Generated outputs use their own reproducible generator and manifest. Preserve
license notices and intentional parser/module fixture bytes.

Validate subsystem moves with a native rebuild and the full runtime suites.
Validate ESM/bootstrap changes with reproducible generated output, builtin identity,
CommonJS/ESM differentials and the native Firefly gates. Verify documentation/source
links after moves. Record each coherent step and outstanding refactor work in
[PROGRESS](../PROGRESS.md); formatting does not establish additional API support.
