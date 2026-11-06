# Modules and packages

Mite implements its own Node-style loader on LibJS. Module resolution is partial;
consult the [matrix](../compatibility/mite-0.1.md) before relying on a Node loader option.

## Choose a module format

| Entry | Interpretation |
| --- | --- |
| `.mjs` | ES module |
| `.cjs` | CommonJS |
| `.js` | Nearest package.json type; no automatic ESM syntax detection |

For an ESM project use package.json with `"type": "module"`, or choose .mjs explicitly.
Relative ESM imports should name their files, such as `./format.mjs`.

```js
import {readFile} from 'node:fs/promises';
const text = await readFile(new URL('./message.txt', import.meta.url), 'utf8');
console.log(text);
```

Static/dynamic imports, live bindings, cycles, top-level await and import.meta
url/filename/dirname/main are implemented in measured cases. Public filename,
dirname and CommonJS module.paths values use native filesystem separators; URLs
use URL separators and escaping. UTF-8 entry arguments are converted explicitly
to native filesystem paths. Unicode entry/dependency/package paths are compared
with Node locally; native Windows acceptance of these fixes remains pending.
UNC file-module URLs are still unsupported on Windows and fail explicitly with
ERR_MITE_UNSUPPORTED_API. On POSIX, a nonlocal file-URL host remains invalid.
Windows module realpaths now use an owned link-aware path walk to preserve normal
component spelling, including 8.3 aliases, while resolving symlinks/junctions.
Exact Node metadata and shared linked-module caches are tested; the new Windows
branch still requires native acceptance. See
[ADR 0064](../architecture/0064-windows-module-realpaths.md).
A top-level await
that never settles exits with status 13. JSON modules require `with {type: 'json'}`.
Unknown import attributes may fail in the engine before host-specific error shaping.

## CommonJS and interoperation

CommonJS supports the wrapper, require, module.exports, relative/directory loading,
require.resolve/cache/main, module metadata and cycles. ESM can consume a CommonJS
module's default/module.exports value. Automatic named CommonJS export detection,
require.extensions and synchronous require(ESM) remain unsupported. Eval mode does
not provide the full CommonJS file wrapper.

For implemented built-ins, node: and ordinary names share the exports object;
prefixing with node: also avoids a local package shadowing the intended built-in.
Fixed ESM named exports exist only for the implemented registry surface.

## npm project layout

Install dependencies with your existing npm workflow. Mite does not contain a package
manager. It resolves local node_modules, scoped packages, legacy main/index, package
exports/imports, conditions and self-references in tested cases. NODE_PATH/global
folders, custom loader conditions, resolve options and module-sync compatibility
remain absent. Recursive package resolution is bounded.

A pure JavaScript package can still depend on an unsupported built-in, platform
service, process event or native addon. Check [measured packages](../compatibility/npm-packages-0.1.md)
and use a small fixture against both Node and Mite. Passing one package entry does
not prove its optional adapters or complete upstream suite.

## Diagnose resolution

Use explicit filenames/extensions, inspect package type and exports/imports, then
reduce to one import/require. Preserve the exact specifier and error code when filing
a report. See [troubleshooting](../guides/troubleshooting.md) and [Node migration](../migration/from-node.md).
