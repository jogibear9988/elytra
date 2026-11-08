# ADR 0143: Snapshot trusted own getters during contextBridge publication

Status: accepted for the locally tested macOS native/production-helper subset.

## Context

The pinned Code-OSS preload publishes getters for process metadata, including env,
versions, platform and execPath. Previously every accessor failed API publication.
Electron's actual reference evaluates those getters while copying the API; the
receiving page observes frozen data snapshots rather than live source accessors.
Enabling getters in untrusted page/IPC/result data would weaken an existing boundary.

## Decision

Add an explicit native accessor reader to the bounded snapshot engine, disabled by
default. Only contextBridge.exposeInMainWorld supplies it for the trusted source API.
Read ordinary own descriptors without hooks. For a getter, require an ordinary
ECMAScript function from the exact source realm; reject foreign, native, Proxy and
bound getters without invoking them. Call with its source object as receiver and
accept the supported returned graph. Setter-only descriptors copy undefined; no
setter is called or exposed. Inherited/custom prototype and native objects keep
their prior restrictions. Module redaction occurs before descriptor enumeration.

Permit at most 64 getter calls per publication, in addition to existing graph,
function and binary budgets. Copy returned data into ordinary frozen destination
properties; do not publish getter descriptors or introduce a live property proxy.
Function/binary values use existing guarded endpoints/copy policy. Root visited
source values as well as the original graph/key vectors: a getter can trigger GC
while transient addresses are still keys in the graph alias table.

Retain local owner/document/realm/registry references through getter execution.
Reject recursive publication while the outer capture is active, preventing nested
captures from bypassing function/publication quota accounting. Recheck world/document
and bridge liveness after getters and before materialization. Check the principal
namespace again after capture so getter side effects cannot overwrite a newly
created property. Failed capture revokes newly created function endpoints and does
not install a partial namespace. Source getter effects themselves are not rolled back.

All other capture paths remain accessor-free: page function arguments, callbacks,
function/Promise results, IPC and native default data snapshots never supply the
reader. Errors are sanitized without inspecting foreign exceptions. No native
process object, filesystem privilege, additional renderer require or new contextBridge
method is introduced. Ladybird and Mite's dependency boundary are unchanged.

## Acceptance

The actual LibJS/LibWeb isolation gate checks source receiver identity, one-time
snapshot/frozen descriptors, environment-shaped data, functions/aliases, cycles,
binary values and getter-triggered GC. It covers exceptions/unsupported results,
foreign/native/Proxy/bound getter refusal without hooks, exact 64-call quota and
65th refusal, function-budget rollback, namespace collision, reentrant publication,
revoke during a getter and copied values surviving that revoke. Existing module
redaction tests include a trusted getter returning the private marked module.
Page arguments and function-result accessors remain rejected without execution.

One focused hidden production-helper fixture checks Code-OSS-shaped metadata
getters, source updates versus published snapshots, correct receivers, frozen
copied properties, a function getter, failed publication, strict other boundaries
and reload. The actual Electron 45.2.3 common reference passes snapshot/descriptor/
receiver/freeze/function/rollback/isolation behavior; Firefly's additional accessor
restrictions for other directions are scoped separately from that reference.
The 64-call/reentry/exact-realm policy is intentional Firefly behavior, not general
Electron accessor parity. Existing callback/Promise/job/DOM/raw-wire gates remain green.

Local macOS Release build and targeted acceptance pass. The new macOS aggregate
and Linux Xvfb gate are wired; fresh hosted Linux/native Windows execution remains
unverified. No broad desktop suite or physical UI automation is used.

## Remaining Code-OSS work

This enables the shape of the published metadata graph, not the actual process
object or full preload. Implement native sandbox process metadata/additionalArguments,
zoom/file APIs and MessagePort handoff with their own end-to-end evidence. The actual
main entry still has the previously observed allowServiceWorkers blocker. Worker/
cache/resource contracts and a real editor open/edit/save acceptance remain required;
no unchanged preload or workbench launch is claimed by getter publication alone.
