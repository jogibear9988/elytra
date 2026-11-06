# Embedded runtime sources

Maintained ESM inputs produce checked-in JavaScript embedding adapters in
`mite/modules/generated`. Mite embeds those factories in its executable and
runs them with LibJS. Neither Node nor npm is linked or launched at runtime.
A normal native build needs Python to check their hashes, but needs no npm install
in this directory. Node v24.15.0 is the current behavioral reference.

To update or regenerate:

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/runtime-js
node tooling/runtime-js/build.mjs
node tooling/runtime-js/build.mjs --check
python3 tooling/check_runtime_js.py
cmake --build --preset dev --parallel 4
ctest --preset dev --output-on-failure
```

Change exact versions in package.json and refresh package-lock.json when updating
packages. Review source, transitive dependencies, license notices, imports and
behavioral differences before committing regenerated outputs. Do not edit
node_modules or generated files to fix compatibility: use our entry adapters.

The generator uses pinned esbuild 0.25.12, bundles readable-stream 4.7.0 and
string_decoder 1.3.0, and redirects their buffer/events/process/string_decoder
imports to Mite's builtins. In particular it bypasses readable-stream's Node entry
that imports the native Node stream module. No buffer/process/events npm fallback
implementation is embedded. abort-controller, event-target-shim and safe-buffer
are bundled transitive sources; exact versions, npm integrity metadata, source
hashes, package license declarations and license-file hashes are in the generated
manifest. Full notices are embedded in each factory and retained as LICENSE.txt
sidecars. esbuild itself is build-only.

`--check` recreates all output and checks exact equality. The Python build gate
checks artifact, notice, lockfile, generator and adapter hashes without installing
npm dependencies; it does not replace regeneration/source review when updating
packages. Keep generated notices with any source redistribution, and include
runtime library notices with binary distributions as well.

See `docs/architecture/0016-portable-streams.md` for integration and known limits.

## Authored ESM and synchronous bootstrap

modules.json maps builtin names to maintained sources under mite/modules and the
initial globals under mite/runtime/bootstrap. Private imports are bundled; every
builtin entry exports one default factory. Factories receive native dependencies
from the embedding loader and construct one realm-local builtin instance.

factories.mjs compiles those entries as ESM, checks the pinned compiler's single
factory export, and emits a synchronous callable expression with strict semantics.
This is the internal embedding ABI, not a public CommonJS implementation source.
It preserves synchronous require and initial global construction while application
ESM/CommonJS keep their existing shared module identities. Only bootstrap.js calls
its generated initializer immediately.

Portable npm adapters are also authored as ESM. Their generated wrapper supplies
only Mite's reviewed host.require imports while constructing the portable library.
Third-party CommonJS is a bundled dependency format, not a Node runtime fallback.

The manifest covers all authored source/helper/catalog hashes, generated files,
compiler/lockfile provenance and existing dependency notices. Normal C++ builds
verify this state with Python and fail on stale ESM edits; regeneration needs the
pinned Node build tools. Git attributes retain these exact bytes on Windows.

## Firefly adapters

`build-firefly.mjs` uses the same factory compiler for the desktop API and DOM
app-region installer. It writes `firefly/core/generated` with an independent
manifest; standalone Mite remains independent of Firefly source and validation.
See [Firefly authoring](../../firefly/core/README.md) for regeneration commands.
