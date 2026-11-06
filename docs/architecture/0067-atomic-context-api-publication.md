# 0067: Atomic publication of nested isolated-world APIs

Status: native implementation and SDK/helper tests; public preload/IPC integration
is still incomplete and remains unsupported in BrowserWindow.

## Decision

Implement the renderer-side ContextBridge publication engine for
exposeInMainWorld(apiKey, api). Follow Electron's
[contextBridge API model](https://www.electronjs.org/docs/latest/api/context-bridge):
copy/freeze data and proxy functions. Compose the existing trap-free graph
snapshot and document-bound FunctionBridge, retaining cycles, sparse arrays and
repeated function identity within a publication. Snapshot capture defaults still
reject functions; only an explicit native registry may capture function tokens,
and materialization validates their exact destination realm. Snapshots retain no
source GC pointers or callable resolvers.

Prepare and freeze the entire destination graph before defining the namespace.
Reject occupied own properties without reading their getters. Define the final
property as enumerable, nonwritable and nonconfigurable, without inherited setters.
On failure revoke every staged endpoint and leave the global unchanged. Bound each
owner to 64 publications/64 function endpoints and namespace keys to 256 UTF-16
units, alongside existing snapshot and per-function argument/pending-call limits.
These are output/registry budgets, not hard containment of source key-enumeration
scratch allocations. Unsupported proxies, bound functions, host objects, accessors,
symbols, promises-as-API-values and custom data prototypes reject explicitly.
Function results can still use the existing guarded native Promise bridge.

Publication requires the exact active source/document and an executing source
owner. Native function Realm metadata alone is insufficient: reject principal
script reentry, including through call/apply while a source owner remains active.
Use weak publisher closures and revoke all function endpoints/pending promises on
source detach; retained frozen data remains safe. Registries must also revoke on
document inactivity. No main-process capability, new dependency or upstream patch
is introduced.

## Evidence and remaining integration

SDK tests cover nested/cyclic data, frozen target prototypes, function aliases,
callback results, trap-free rejection/rollback/function budgets, occupied getters,
inherited setters, deliberately leaked publisher calls, principal reentry,
pending-Promise rejection and retained function denial after detach/GC.
The existing weak-filter test additionally requires an empty native retaining-path
observation before collection and retains both original weak-realm/native-filter
collection assertions. This separates root inspection from the collector's
conservative register snapshot; no collector/runtime change or retry is added.

The actual sandboxed helper publishes a harmless compiled fixture API before first
author script, exercises top-level/srcdoc documents, and verifies removed-frame
function/Promise revocation with a continuing parent API. The original helper is
a negative control. Fixture publication excludes transient initial about:blank:
it can reuse its Window for srcdoc, where a second immutable namespace is correctly
rejected. This is an explicit fixture document policy, not a public navigation
policy or a claim that about:blank preloads work.

Before exposing the public API, integrate application-selected preload sources,
dedicated production helper selection/staging, document/window/navigation policy,
bounded main/renderer IPC, malicious wire-input checks and Electron migration
fixtures. Do not select the diagnostic helper for applications or enable public
preload merely because this engine layer passes. Arbitrary isolated worlds,
executeInMainWorld, callback arguments, broad structured-clone types and full
Electron error behavior remain separate work.
