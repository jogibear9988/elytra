# 0045: Guard native NodeFilter invocation through return conversion

Status: internal macOS ARM64 SDK/helper slice. Public preload/contextBridge/IPC and
cross-world traversal property access remain incomplete.

## Decision

TreeWalker and NodeIterator enter WebIDL call_user_object_operation through the
native Bindings::accept_node helper. That path creates the callback-world Node
wrapper, looks up acceptNode (including getters/proxies), invokes the callback and
converts its result with ToInt32. Protect this whole entry rather than only the
function call: wrapping a revoked world or executing a return object's valueOf
outside its owner frame would bypass lifetime checks.

Substitute the native NodeFilter TU with an owned adapter in our LibWeb graph.
A generated include renames exactly the accept_node definition; internal traversal
calls keep its public guarded name. All other upstream bytes, class layouts,
traversal algorithms and license notices remain untouched. Require one substituted
TU, explicit generation dependencies, exact entry count and source-preservation/
drift tests. Candidate pin updates must review the semantic seam.

WorldJobGuard resolves associated and actual callable realms, checks revocation
before wrapping/lookup and retains registered owners through native conversion.
Logical detach and last-owner release during getter/callback/conversion defer
physical cleanup until this frame finishes. Abrupt owned completions become a
fixed caller-realm TypeError under a synthetic context, without raw Source values
or Source stack frames. This is an intentional exception-identity difference for
registered filters, including a same-world caller. Unregistered callbacks keep
ordinary exceptions/identity. Principal filters invoked below Source execute the
whole native helper in the principal context scope from ADR 0044, preserving page
stack privacy and Promise-job ownership through getters and numeric conversion.

## Validation

SDK checks use actual native NodeIterator/TreeWalker wrappers across page and Source
realms. Own callback identity, receiver, Node wrapper realm and object-valued numeric
returns work. Raw thrown Source objects and throwing valueOf results become generic
page TypeErrors. After detach and GC, a retained native iterator rejects before
acceptNode lookup, without advancing Source counters. A getter detaches/drops the
last external owner and forces GC: its returned function and valueOf still finish
DOM work, then the world physically detaches and the weak owner expires.

A principal filter's getter, callback and valueOf expose no Source frames when
called below Source; all three page Promise jobs survive Source detach. The sandboxed
helper checks native function/receiver/wrapper behavior, proxy lookup errors and
ordinary principal error identity. An iframe removes itself during Source result
conversion; synchronous DOM work and traversal finish, bridge output is denied and
the queued Source job stays silent. Ordinary renderer/compositor/process and
Firefly/Electron reference regressions remain required because LibWeb is shared.

## Limits

This guards native invocation, not an entire traversal-object membrane. Native
TreeWalker/NodeIterator .filter getters still return raw callbacks across wrapper
worlds; their property policy must be implemented before public preload. Native
traversers may retain filter callbacks/realms after detach. Arbitrary direct calls,
other user-object operations, callback constructors, generic WebIDL return modes,
shared registries, navigation/BFCache, workers/finalization and hard bounds remain
open. Public preload/contextBridge/IPC stay unsupported. No full isolation/audit,
complete reclamation, new platform/package or visible-desktop claim. Standalone
Mite and Ladybird's pin/source remain unchanged; existing build warnings persist.
