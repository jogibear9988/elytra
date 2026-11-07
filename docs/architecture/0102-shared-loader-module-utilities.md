# ADR 0102: ESM-owned require contexts reuse the native loader

Status: implemented and locally accepted on macOS ARM64; new Linux/Windows
execution pending. See the [API contract](../api/module.md).

The pinned Code-OSS bootstrap-node module calls createRequire(import.meta.url).
Provide that general Node-compatible utility rather than rewriting dependencies
or creating another evaluator/cache. Keep URL/path validation in an authored ESM
factory, compiled by the existing pinned build pipeline. Native C++ creates the
synthetic parent and delegates to ModuleLoader::make_require.

Extract common module metadata creation so ordinary file modules and ESM-owned
contexts use the same parent/children/path/main behavior. Only actual file modules
enter the CommonJS cache or execute source. Generated require and resolve closures
capture their synthetic parent through LibJS's traced native-function closure;
do not add a permanent global root or cache dummy modules. Actual collection tests
retain each function independently and then resolve/load modules and inspect the
synthetic requireStack.

Build builtinModules and isBuiltin from the implemented registry, not the wider
reserved Node-name inventory. Enumerating an unavailable builtin would misleadingly
advertise it. Reserved unsupported names still fail explicitly rather than falling
through to npm packages. This deliberate inventory difference is documented and
every listed module is loaded through both ESM and CommonJS in acceptance.

The default facade is a utility object. The Module constructor, loader hooks and
compile-cache APIs remain absent; neither a disabled-looking fake cache nor V8 is
introduced. The Code-OSS compile-cache/diagnostic path needs a separate explicit
LibJS adaptation. This slice removes a real createRequire dependency, not the
complete application bootstrap barrier. Mite remains LibJS-only and independent
of Firefly/LibWeb.
