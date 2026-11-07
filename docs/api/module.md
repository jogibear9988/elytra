# node:module

Status: partial utility and synchronous resolve API. createRequire, isBuiltin and builtinModules have local
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

require.resolve options, require.extensions, global lookup folders, load hooks,
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

## registerHooks(options)

Register a synchronous **resolve** hook and return an object with an idempotent
`deregister()` method. Later registrations execute first. Local macOS Node/Mite
and real embedded-host GC tests pass; native Linux passes at 6c613ec and
Windows at a56d183. See [current evidence](../status.md).
This is a partial API: passing a load function rejects the entire registration
with ERR_MITE_UNSUPPORTED_API before installing its resolve function.

```js
import {registerHooks} from 'node:module';

const hooks = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'app-config') {
      return nextResolve('./config.mjs', context);
    }
    return nextResolve(specifier, context);
  }
});
const config = await import('app-config');
hooks.deregister();
```

Hooks apply to subsequent dynamic imports, static edges in modules loaded after
registration, require and require.resolve, including createRequire. Static imports
in the registering module have already resolved. Multiple hooks form a LIFO chain;
nextResolve calls the next hook or the ordinary native resolver. Omitting its context
keeps the current context; supplied properties merge over it. The hook can change
the specifier, parentURL or package conditions. Native conditions are node/import/
node-addons for ESM and require/node/node-addons for CommonJS; module-sync remains
absent because synchronous require(ESM) is unsupported.

Context includes parentURL (a file URL) and conditions. ESM also receives
importAttributes; CommonJS omits it. Hooks can return a different importAttributes
object, limited to the existing JSON type contract. Merely changing nextResolve's
context attributes does not replace the final result attributes, matching the
Node reference. No general attribute-dependent cache variants are implemented.

Return a synchronous object with an absolute url. Call nextResolve or return
shortCircuit:true. Optional format hints must agree with the native module type;
transforms/overrides require unsupported load hooks. Supported targets are actual
local file modules, builtins and registered native host modules; ordinary module
cache identity is preserved. Unsupported protocols/private bridge URLs fail
explicitly. Native addon and require(ESM) restrictions remain in force.

Deregistration removes the hook from future chains, preserving an in-flight
snapshot. A saved nextResolve remains usable after its original call and removal;
it captures realm-owned state rather than a native stack frame. The loader roots
active hooks until deregistration or VM shutdown. This adds no privileged loader
to a renderer or preload.

| Failure / limit | Result |
| --- | --- |
| Null/missing options or invalid hook function | TypeError; invalid function has ERR_INVALID_ARG_TYPE |
| Promise/primitive/missing URL or broken continuation chain | TypeError; some malformed-result codes/messages differ from Node |
| Relative/invalid result URL | ERR_INVALID_URL |
| Non-file protocol or private shim target | ERR_UNSUPPORTED_ESM_URL_SCHEME |
| load function or mismatching format override | ERR_MITE_UNSUPPORTED_API |
| More than 64 active resolve hooks or 1,024 conditions | ERR_MITE_RESOLVE_HOOK_LIMIT |

No asynchronous register, load/source transformation, --import/--require preload
CLI, worker inheritance or ASAR filesystem support is supplied. See
[ADR 0109](../architecture/0109-synchronous-resolve-hooks.md).

## Remaining module APIs

The Module constructor, asynchronous register, findPackageJSON, compile-cache APIs and
syncBuiltinESMExports are not exported. In particular this does not supply a V8
compile cache or allow the complete Code-OSS bootstrap to run. Its cache/diagnostic
imports use an explicit application-only [LibJS cache policy](../architecture/0106-code-oss-libjs-cache-policy.md),
not new Mite builtin exports; see the
[Code-OSS inventory](../compatibility/vscode.md).

Reference: [Node 24.15 module API](https://github.com/nodejs/node/blob/v24.15.0/doc/api/module.md).
