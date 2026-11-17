# ADR 0203: Genuine CommonJS Module interception

Status: implemented partial native Module surface; five Node/Mite/embedding-host
and rebuilt Firefly differential groups pass with real GC on macOS ARM64.
A controlled original Code-OSS CommonJS workflow passes offscreen on macOS;
fresh platform and general desktop acceptance remain pending.

## Demand

Real CommonJS consumers, including the original Code-OSS API interceptor, patch
Module._load and its resolver rather than only registering an ESM hook. An empty
Module constructor or a replacement require supplied only inside a test would
allow imports without executing the application's actual load path. The existing
native loader already owns files, package conditions, cache entries and cycles.

## Decision

Return a genuine constructible Module as the shared node:module/default/CommonJS
export and use its prototype on actual native-loaded CommonJS parent records.
Keep constructor/loader state realm-local and GC-traced. Native require closures
read their fixed parent's live require method; the prototype base calls the live
Module._load. Do not install the lexical wrapper back as the module's own require,
which would recursively call itself.

For ordinary requests, the default _load calls the live _resolveFilename before
private native loadResolved. Wrappers therefore run before native resolution/cache lookup, can
return actual application API objects and can delegate through captured originals.
The native load continues to publish a provisional record before evaluation,
serve partial exports in cycles, preserve object identity and roll back failed
loads. Root cached modules and original exceptions across parent-child accessors,
cleanup, allocation and real garbage collection.

Use the public lookup methods in actual native package resolution. _nodeModulePaths
computes native ancestor roots; parent.paths, explicit globalPaths and options.paths
control real lookup instead of decorative metadata. Final native lookup arrays
are limited to 1,024 string paths and reject NUL; package main/exports and existing
resolve-hook conditions remain with the same resolver. This does not discover a
Node installation's global directories or implement all Node resolution options.

Retain the existing ESM/CommonJS namespace translator and its source analysis.
Author interception does not join the private CJS/builtin/host shim identity sets.
Data modules, synchronous require(ESM), unsupported builtins and native addons keep
their explicit default-loader restrictions. An application wrapper returning an
ordinary object is a real intercepted result, not a claim that those native
capabilities were implemented.

## Compatibility boundaries

This is not the complete Node Module internal API. _compile, _findPath, extensions,
load hooks, asynchronous registration, V8 compile caches and automatic global
folder/NODE_PATH discovery remain outside scope. options.conditions is explicitly
unsupported; registerHooks condition contexts retain their existing supported path.
Arbitrary cache/proxy mutation timing and full Windows behavior are not certified.

Bare builtin cache shadows remain distinct from node:-prefixed loads, which
bypass the mutable filename resolver and cache while preserving registered native
resolve hooks. Resolve and load use separate native intent: an unknown prefixed
resolve reports MODULE_NOT_FOUND, while a real builtin load reports
ERR_UNKNOWN_BUILTIN_MODULE. No caught exception is rewritten to simulate this
behavior. Lookup roots are real filesystem inputs, not a privilege
boundary or a new renderer API. Standalone Mite gains no Firefly/LibWeb/Node/V8
dependency, and Ladybird remains unmodified.

## Validation and remaining work

Five shared groups pass in Node 24.15, standalone Mite, the native embedding host
and rebuilt Firefly-as-Mite, all with actual --expose-gc. They cover nested live
_load wrappers/receivers/parents, virtual results before resolution, real cached and
circular dependencies, exception-preserving rollback/retry, retained original
loads consulting a later resolver, and actual options.paths/parent.paths lookup.
Existing module utilities, resolve hooks, CommonJS, namespace-GC/reentry and data
module tests also pass.

The SDK/offscreen build and embedded Firefly differential controls pass. The
original controlled Code-OSS CommonJS interceptor now passes nested/repeated API
requires, activation and its command, document open/show, edit/save and independent
physical-file readback. Evidence is retained in
`build/codeoss-commonjs-{acceptance.log,evidence.json,workbench.log}`. The saved
file SHA-256 is `9cec19ed5d8e80b0de57bbd7ddc4cb7df2aedd5831784b11177cc8d6ac24b439`.
The final binary's CJS repeat exits 0; the harness immediately sends two
authenticated helper-cleanup signals, after which all 13 observed native processes
are independently confirmed absent by birth/executable identity. An earlier CJS predecessor had zero cleanup signals, not the final run.
The preceding SDK binary's ESM control also passes the workflow, with exit 0 and
two helper-cleanup signals before all 12 observed processes are absent. Neither
recorded control may be reported as an entirely natural teardown. Immediate harness
cleanup is not by itself proof of a process leak or runtime failure.

A focused regression caught unknown prefixed resolution returning its name instead
of MODULE_NOT_FOUND. The final native intent distinction fixes resolve versus load
without rewriting hook exceptions. The rebuilt standalone/embedding/Firefly five-group
controls pass again, as do eight existing regression CTests including builtins and
packages. The final CJS application repeat uses this exact corrected binary.

This is controlled macOS offscreen API acceptance, not visible painting/input,
PTY, every extension, general native-addon support or full VS Code parity.
Fresh Windows/Linux dispatcher results and wider Module edge cases remain pending.
See the [API contract](../api/module.md#module-constructor-and-commonjs-dispatch) and
[compatibility matrix](../compatibility/mite-0.1.md).
