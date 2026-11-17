# node:module

Status: partial native Module constructor, CommonJS dispatch and synchronous
resolve API. The new interception/lookup slice passes five real Node/Mite groups
with `--expose-gc`, the native embedding host and rebuilt Firefly's embedded runtime
on macOS ARM64. Existing module/hook/CJS/data regressions also pass. Older utility
APIs have Linux/Windows evidence at 678c85e; that does not validate the new dispatcher
on those platforms. A controlled original Code-OSS CommonJS activation/command/
open-edit-save workflow also passes offscreen on macOS; visible input, PTYs and
arbitrary-extension acceptance remain open. See [current evidence](../status.md).

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
is the actual constructible Module function. Module.Module is that same function,
and native-loaded CommonJS parent records use its prototype.

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

require.resolve supports the actual options.paths lookup described below.
require.extensions, automatic Node global-folder/NODE_PATH discovery, load hooks
and synchronous require(ESM) remain unsupported. Source-analyzed CommonJS named exports
are described below.
Native Windows current-drive rooted paths and trailing backslashes pass the
678c85e native gate. Existing platform-specific loader limits,
including UNC module URLs, still apply.

## Module constructor and CommonJS dispatch

The default export, require('module'), require('node:module') and Module.Module
share the same constructor. `new Module(id, parent)` creates a real CommonJS
metadata object with exports, children, parent and loaded state. Loaded file
records also carry their actual filename/path and lookup paths; createRequire
uses a real but uncached synthetic parent. This does not implement every Node
Module internal such as _compile or _findPath.

`Module.prototype.require` consults the current Module._load on each call. Native
lexical require functions consult their actual parent's current require method,
so instance/prototype overrides and nested wrappers take effect before default
resolution or cache lookup. A wrapper can return a genuine virtual-module export
or delegate to the captured original. For ordinary requests, the default _load
consults the current _resolveFilename and then performs the native load. Prefixed
node: requests use the native builtin path without the mutable filename resolver
or cache, while registered native resolve hooks still participate. Unknown
require.resolve('node:missing') reports MODULE_NOT_FOUND; loading that unknown
builtin reports ERR_UNKNOWN_BUILTIN_MODULE. This is not an import-only stub.

```js
import Module, {createRequire} from 'node:module';
const require = createRequire(import.meta.url);
const original = Module._load;
const api = {answer: 42};
Module._load = function(request, parent, isMain) {
  if (request === 'application-api') return api;
  return Reflect.apply(original, this, arguments);
};
try {
  console.log(require('application-api') === api); // true
} finally {
  Module._load = original;
}
```

Native caching and cycles remain authoritative: a module is published before
executing its body, cycles see its partial exports, repeated loads reuse the actual
exports, and a failed load removes its provisional cache/parent-child entry.
Original exception identity survives cleanup and collection. Cached parents and
retained require functions remain GC-traced. Module._cache is the real native
CommonJS cache; replacing it is consulted by subsequent default loads. Existing
require.cache references retain the object assigned when those functions were made.
Bare builtin cache shadows are supported; node:-prefixed builtin loads bypass that
cache. Neither dispatcher invents an ESM namespace or bypasses native-addon/data/
synchronous-ESM restrictions.

## CommonJS lookup paths

`Module._resolveFilename(request, parent, isMain[, options])`,
`Module._resolveLookupPaths(request, parent)` and `Module._nodeModulePaths(from)`
are live methods used by the default loader. Returned paths govern actual package
lookup. Module._nodeModulePaths computes native ancestor node_modules directories;
parent.paths and explicitly supplied Module.globalPaths contribute real roots.
Mite does not discover a Node installation's global directories automatically.

`require.resolve(request, {paths: [directory]})` and the corresponding
_resolveFilename option search actual roots without modifying the supplied array.
Relative requests use those directories directly; package requests expand through
node_modules ancestors and normal package main/exports rules. Default builtin
resolution does not consult filesystem lookup paths/options. Native admission
allows at most 1,024 final lookup paths, with string/NUL validation; exceeding it
uses ERR_MITE_MODULE_PATH_LIMIT. Invalid arrays use ERR_INVALID_ARG_VALUE, invalid
elements ERR_INVALID_ARG_TYPE. options.conditions remains explicitly unsupported
with ERR_MITE_UNSUPPORTED_API; this is separate from supported registerHooks
condition contexts.

These are tested partial internal APIs, not complete Node loader parity.
Unsupported Module internals, arbitrary cache/proxy mutation timing, automatic
global-folder discovery and full Windows path behavior remain outside this slice's
acceptance. See [ADR 0203](../architecture/0203-commonjs-module-interception.md).

The original controlled Code-OSS CommonJS interceptor now passes nested/repeated
require('vscode'), extension activation and its registered command, followed by
openTextDocument/showTextDocument and WorkspaceEdit/save with independent physical
file readback. The final binary's CJS repeat exits 0; the harness then sends two
authenticated helper-cleanup signals, and all 13 tracked native processes are
independently confirmed absent. An ESM control on the preceding SDK binary also
passes its workflow, with exit 0 and two cleanup signals before its 12 tracked
processes are absent. Full natural teardown is unaccepted; this immediate harness
cleanup is not itself evidence of a leak or runtime failure. These are local macOS
offscreen API workflows, not visible editor input, PTY, broad native-addon/extension
or all-platform acceptance.

## CommonJS named exports in ES modules

Local macOS Node24.15 differential acceptance covers static/dynamic namespace imports
from CommonJS files. Discovery uses pinned cjs-module-lexer 2.1.1's pure JavaScript
provider on LibJS; it needs no Wasm, Node host or V8. A guarded build-time adapter
retains already recognized names when a later unsupported getter appears, matching
measured Node24 native-lexer behavior. Original dependency bytes, hashes and MIT
notices remain preserved in the generated artifacts.

```js
// legacy.cjs: exports.answer = 42;
import legacy, { answer } from './legacy.cjs';
console.log(answer, legacy.answer);
```

Source analysis recognizes the provider's static assignment/property/transpiler
patterns, including literal bracket names. Comments, strings, templates and regular
expressions cannot fabricate exports; computed property names are not enumerated at
runtime. Reexport analysis follows real CommonJS resolution, unions relative/package
CommonJS names and breaks cycles without executing application modules. Builtin,
JSON and ESM reexports do not automatically add names. Unresolved reexports do not
invent names. A missing named import fails linking before CommonJS side effects.
Reexport analysis has an explicit depth limit of 512.

At evaluation, each namespace receives snapshots of recognized own properties from
the actual completed CommonJS export value. A getter can run; a getter exception
leaves that named value undefined, while own-property trap failures propagate.
Unrecognized getters are never invoked merely to discover names. The default and
'module.exports' markers reference the exact whole export value; CommonJS properties
with those names cannot shadow the namespace markers. Function exports and relative
cycles preserve real require/import object identity. Named snapshots do not change
when the default object later mutates.

Replacing require.cache[file].exports changes ordinary require results; the first
ESM import retains the value produced by successful CommonJS evaluation. An actual
cache deletion/reload refreshes that original value. Arbitrary source-file edits
and resolver/cache-hook changes after analysis are not yet accepted. Full Node
syntax-detection, synchronous require(ESM), native-addon or loader-hook parity does
not follow from these namespace checks. Fresh native Linux/Windows validation of
this slice remains open.

A resolve hook that recursively imports the very CommonJS URL currently being
analyzed cannot publish a second native module record. Mite reuses the first
namespace, whose names reflect the analysis available at its creation. Node24
rejects the measured case with ReferenceError; Mite's stable first-record behavior
is an intentional difference, not complete reentrant-analysis parity.

The unchanged compiled Code-OSS ZIP writer now runs with genuine yazl dependencies;
independent ZIP CRC/extracted-byte checks pass. This accepts that consumer, not
extension installation or a complete VS Code workbench.

## isBuiltin(name)

Return true for an available Mite builtin, with or without node: prefix. Return
false for non-strings, unknown names and unavailable Node modules. For example,
fs and node:fs are true; unavailable v8 and http2 remain false. This is an
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

Context includes parentURL (a file or data URL for ESM; a file URL for require)
and conditions. ESM also receives
importAttributes; CommonJS omits it. Hooks can return a different importAttributes
object, limited to the existing JSON type contract. Merely changing nextResolve's
context attributes does not replace the final result attributes, matching the
Node reference. No general attribute-dependent cache variants are implemented.
The new data target/parent paths pass local macOS Node/Mite differential and real
GC controls in standalone Mite and rebuilt embedded Firefly. An original controlled
Code-OSS extension fixture also passes activation, a command, document open/show
and edit/save with independent physical-file readback and clean native teardown.
This does not establish visible editor, PTY, arbitrary-extension/native-addon or
fresh Windows/Linux acceptance; see [data-module evidence](data-modules.md#validation).

Return a synchronous object with an absolute url. Call nextResolve or return
shortCircuit:true. Optional format hints must agree with the native module type;
transforms/overrides require unsupported load hooks. Supported targets are actual
local file modules, [data-URL JavaScript/JSON modules](data-modules.md), builtins
and registered native host modules; ordinary module cache identity is preserved.
Data-parent resolution supports builtins and absolute URLs; relative/package
imports fail without a file base. Data JSON still requires its type attribute. Unsupported protocols/private bridge URLs fail
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
| Unsupported URL scheme or private shim target | ERR_UNSUPPORTED_ESM_URL_SCHEME |
| Data target via CommonJS require / require.resolve | ERR_MITE_UNSUPPORTED_REQUIRE_ESM |
| Data URL/source/cache/record bound | ERR_MITE_DATA_MODULE_LIMIT |
| load function or mismatching format override | ERR_MITE_UNSUPPORTED_API |
| More than 64 active resolve hooks or 1,024 conditions | ERR_MITE_RESOLVE_HOOK_LIMIT |

No asynchronous register, load/source transformation, --import/--require preload
CLI, worker inheritance or ASAR filesystem support is supplied. See
[ADR 0109](../architecture/0109-synchronous-resolve-hooks.md).

## Remaining module APIs

Asynchronous register, findPackageJSON, compile-cache APIs and
syncBuiltinESMExports are not exported. Constructible Module and its partial
CommonJS dispatch surface are described above. This does not supply a V8 compile
cache or certify every Code-OSS bootstrap/feature path. Its cache/diagnostic
imports use an explicit application-only [LibJS cache policy](../architecture/0106-code-oss-libjs-cache-policy.md),
not new Mite builtin exports; see the
[Code-OSS inventory](../compatibility/vscode.md).

Reference: [Node 24.15 module API](https://github.com/nodejs/node/blob/v24.15.0/doc/api/module.md).
