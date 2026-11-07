# node:module

Status: partial utility API. createRequire, isBuiltin and builtinModules have local
macOS ARM64 Node differential and actual embedded-host GC tests. Native Linux and
Windows x64 also pass at 678c85e (all 68 runtime suites). See
[current evidence](../status.md).

```js
import {createRequire, isBuiltin, builtinModules} from 'node:module';

const require = createRequire(import.meta.url);
const legacyDependency = require('./legacy-dependency.cjs');
console.log(require.resolve('./legacy-dependency.cjs'));
console.log(isBuiltin('node:fs'), builtinModules.includes('fs'));
```

This supports ESM applications consuming existing CommonJS dependencies; application
examples should continue to use modern ESM. The ordinary module and node:module
names share one default object and the same named functions. Mite's default export
is a utility object, not Node's constructible Module class.

## createRequire(filename)

Accept an absolute native filename, file URL string or the runtime's URL object.
Return a new require function based at that location, without reading or executing
the base file. It may name a nonexistent file. A trailing directory separator
means a directory context; internally use an uncached synthetic parent filename.
File URL search/fragment components do not become part of the filesystem path.

The returned function uses the existing CommonJS resolver and cache, including
relative/directory modules, node_modules, package imports/exports and require
conditions. Its resolve/cache/main properties have the same supported behavior as
ordinary CommonJS require. An ESM entry has no require.main; a CommonJS entry keeps
its actual main module. Missing modules carry a requireStack beginning at the
synthetic parent's filename.

Separate generated require functions share loaded module instances. CommonJS
dependencies and ESM imports share their default/module.exports value; JSON imports
and require share the JSON cache. Deleting a cache entry permits an ordinary reload.
The synthetic parent itself is never inserted into require.cache. Native function
closures keep their context traceable by the GC, including when only resolve is
retained; no permanent synthetic-module root is added to the loader.

| Failure | Result |
| --- | --- |
| Missing, relative, non-string/non-URL, malformed or non-file URL base | TypeError, ERR_INVALID_ARG_VALUE |
| Base contains NUL | TypeError, ERR_INVALID_ARG_VALUE; deliberate early rejection rather than passing a truncated native path |
| Non-string ID passed to require or resolve | TypeError, ERR_INVALID_ARG_TYPE |
| Missing dependency | MODULE_NOT_FOUND with synthetic requireStack |
| Reserved but unimplemented Node builtin | ERR_MITE_UNSUPPORTED_BUILTIN; no local npm fallback |
| Native addon / synchronous require(ESM) | Existing explicit unsupported error |

require.resolve options, require.extensions, global lookup folders, loader hooks,
automatic named CommonJS exports and synchronous require(ESM) remain unsupported.
Native Windows current-drive rooted paths and trailing backslashes pass the
678c85e native gate. Existing platform-specific loader limits,
including UNC module URLs, still apply.

## isBuiltin(name)

Return true for an available Mite builtin, with or without node: prefix. Return
false for non-strings, unknown names and unavailable Node modules. For example,
fs and node:fs are true; v8, http2 and worker_threads remain false. This is an
intentional inventory difference from Node, which provides those modules.

## builtinModules

A frozen sorted array of available bare builtin names, generated from the same
registry that loads them. Each listed name has both ESM and CommonJS load checks.
The wider list of reserved Node names is not a support inventory. Reassigning a
facade property cannot change the native resolver's registry or reserved-name
denial policy.

## Remaining module APIs

The Module constructor, loader hooks, findPackageJSON, compile-cache APIs and
syncBuiltinESMExports are not exported. In particular this does not supply a V8
compile cache or allow the complete Code-OSS bootstrap to run. Its cache/diagnostic
imports use an explicit application-only [LibJS cache policy](../architecture/0106-code-oss-libjs-cache-policy.md),
not new Mite builtin exports; see the
[Code-OSS inventory](../compatibility/vscode.md).

Reference: [Node 24.15 module API](https://github.com/nodejs/node/blob/v24.15.0/doc/api/module.md).
