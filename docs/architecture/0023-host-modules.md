# 0023: Register host modules in the embedded loader

Status: native host-module registration and CJS/ESM gate implemented. The actual
firefly desktop module and its native window integration remain required.

## Decision

Expose ModuleLoader::register_host_module to trusted C++ hosts. A host registers a
realm-owned exports object and an explicit list of named exports during its
initialization, before application entry executes. Keep registration local to the
loader and root exports for that loader's lifetime. No Firefly names, platform
services or desktop dependencies enter Mite's builtin table or standalone build.
There is no application JavaScript API for registering privileged modules.

Registered modules resolve by their exact bare name before node_modules and
package self resolution, including package imports aliases. CJS require returns
the registered object; require.resolve returns the bare host name. ESM default
is that same object. A fixed ESM shim exposes the declared named properties as
snapshots at first module evaluation, with a cached namespace; nested exported
objects retain their identity and mutations. This matches Mite's existing builtin
shim approach, not general live synchronization of changing exports properties.
An empty named-export list provides a default-only namespace. Unknown named
imports fail at link time rather than returning fabricated values.

Use a separate registry and internal mite-host URL identities. The node: namespace
remains reserved for Node builtins: registering a host module does not create a
node: alias or permit replacing a supported/unsupported Node builtin. Direct
application imports of mite-host URLs are rejected. The getter bridge is available
only to the exact generated host-shim module records, not to ordinary modules;
registering one module does not expose access to other host exports. CJS
require.cache entries cannot override registered host objects. This is an
intentional host-module difference from Mite's bare Node builtin cache behavior.
The exports object itself remains mutable unless the host freezes it.

The native registration API currently accepts unscoped ASCII module names starting
with a letter and then letters/digits/underscore/hyphen. Named exports must be
ASCII identifiers, excluding default and duplicates. Reserved words such as class
are valid export names: JSON-quoted property access and export aliases avoid code
interpolation. Invalid registration fails before inserting an entry; duplicate
registration fails rather than changing an existing module's identity. Scoped
host names and arbitrary Unicode export names require a later explicit extension.

## Verification and limits

The actual embedded host registers test modules during initialization. Test native
validation failures for invalid/reserved names, duplicate modules/exports, invalid
export names and a source-injection-shaped name, then retry a valid registration
to check atomicity. Load real CJS/ESM files through a temporary package tree with
a conflicting local package and a package imports alias. Check shared default and
named-object identities, namespace caching, default-only exports, reserved-word
exports, snapshots, callback completion and cache-poisoning resistance.

Force a real GC before first requiring an object held only by the registry, then
load it again through ESM after another GC. Reject direct internal URLs/bridge
imports, CJS/ESM node: aliases, JSON import attributes and missing named exports.
Run the standalone CLI against the same tree: ordinary packages must still load,
and host-only modules must remain missing. Run the complete Mite regression and
inspect standalone linkage after changing module resolution.

These are native macOS ARM64 loader/embedding checks, not Electron migration
fixtures or proof of renderer IPC isolation. Windows/Linux native execution is
unverified. Renderer realms do not receive these host registrations; the future
Firefly host must still implement and test preload/contextBridge and a bounded,
validated renderer IPC boundary. Ladybird remains unmodified.
