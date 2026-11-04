# ADR 0005: CommonJS execution and ESM interoperation

Status: implemented for file modules; package resolution is the next slice.

Execute CommonJS source inside the Node-style function wrapper with exports,
require, module, __filename and __dirname parameters. Use LibJS to parse and run
the wrapper; never invoke Node or transpile ESM into CommonJS. Strip BOM/shebang
before wrapping and preserve script line numbering. Entry `.js`/`.cjs` files use
this host unless selected as ESM by the package type or `.mjs` extension.

Publish a module object in the shared JavaScript `require.cache` before execution
so cycles observe partial exports. `require.main`, module metadata, parent/children,
cache deletion, retry after a thrown exception and `require.resolve` are exercised
against Node. Resolve relative/absolute files, `.js`/`.json` extension fallback and
directory main/index entries. Bare packages, module.paths and resolve options are
not implemented in this slice. Native addons and synchronous require of ESM fail
explicitly. Native ESM syntax auto-detection is also still pending.

CommonJS modules imported from ESM use a native ESM shim over a private synthetic
module exporting a native loader function. The shim calls the loader only during
module evaluation. Loading the graph therefore does not execute CommonJS early.
The internal specifier is accepted only from shims created by the host. Export the
value as default and the current Node `module.exports` namespace marker; static
named-export detection remains unsupported and link errors are explicit.

JSON without a query/fragment shares the CommonJS cache and parsed value between
require and ESM imports. Use LibJS JSONObject parsing and a synthetic default
export. Query/fragment JSON module identities remain separate parsed copies using
upstream `parse_json_module`. The tests cover both directions of cache sharing.

Associate source URLs with GC-owned metadata through LibJS's host_defined field.
Functions keep their script and its metadata alive through normal engine edges.
Avoid permanently rooting every parsed CommonJS wrapper: otherwise deleting
require.cache during hot reload would accumulate unreachable scripts. ESM records
remain rooted in their identity cache, matching the intended long-lived ESM cache.

Current limitations: no CJS globals for CLI eval, no require.extensions hooks,
no static named CJS export analysis, and no synchronous require of ESM. These are
tracked compatibility gaps, not completed Node API coverage.
