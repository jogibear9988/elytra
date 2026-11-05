# 0028: Copy bridge data through an owned snapshot

Status: internal LibJS adapter and macOS ARM64 SDK/helper tests implemented.
Public contextBridge/preload/IPC remain unsupported.

## Decision

Add BridgeData beside IsolatedWorld. Capture into an owned C++ graph containing
only type tags, doubles, UTF-16 strings, property records and node indices. Keep
no VM pointer, JS value, getter, function or descriptor in the completed snapshot.
Reconstruct every container in the target realm, populate its own properties and
then freeze every container. Never return a source object as a copied value.

Initially accept undefined, null, booleans, numbers, strings, exact engine plain
objects and arrays. Containers must belong to the declared source realm, have
that realm's normal Object/Array prototype (or null for ordinary objects), and
have no host class or intrinsic accessors. Reject custom prototypes, proxies,
DOM/host objects, functions, promises, symbols, symbol properties, BigInts,
typed arrays, Maps, Dates and accessors explicitly. These are internal policy
choices pending the public Electron compatibility implementation, not claims of
Electron parity or permanent exclusions from the project scope.

Read ordinary own data descriptors without invoking getters, coercions or proxy
hooks. Only exact engine arrays use an internal descriptor read for their magical
length; the ordinary descriptor helper does not expose it. Prototype contents are
never enumerated or copied. Allocate containers before their properties to preserve
cycles and shared identities. Preserve array holes/length, null prototypes,
enumerability, undefined, negative zero, NaN/infinities and UTF-16 code units.
Define own properties directly, including __proto__, bypassing target setters.
Writable/configurable source attributes are replaced by frozen copy semantics.

Default budgets: 4096 values, 4096 properties, 65536 cumulative string code units
(keys included), depth 64 and array length 4096. Shared containers count once
toward the value budget; repeated edges still count as properties. Reject an
over-budget capture without publishing partial data. Limits are trusted native
configuration, not renderer-provided options. This is not a wire format/decoder.

The SDK enumerates own keys into a temporary vector before the property budget
can be enforced. Snapshot/output size is bounded, but enumeration scratch space
and engine allocation failure are not hard resource containment boundaries.
Public untrusted bridge/IPC must not assume this adapter alone solves resource
exhaustion. No upstream mutation or private engine-layout traversal was added.

## Evidence and remaining work

The actual isolated-world native gate copies in both directions, including into a
page with polluted prototypes and a throwing inherited setter. It checks independent
identity, cycles/aliases, sparse arrays, special numbers, lone surrogates/NUL,
null prototypes, nonenumerable properties, __proto__, target intrinsics and deep
freezing. Delete the source and force GC before reconstruction. Check rejection of
active/exotic/foreign-realm containers with zero getter/proxy-trap calls, cumulative
and boundary budgets, recovery after rejection and GC after reconstruction.

The actual separate helper now uses this adapter for a compiled harmless data
probe. Mutate its source after copying; first author scripts in top-level/srcdoc
documents must still see the original frozen value and their own intrinsics.
Source globals remain private. The unchanged helper negative control and 70-frame
cleanup cycle gate still pass. No application-supplied script or privileged API
is enabled, and the normal Firefly app still uses its original helper.

Next implement generation-aware function/Promise proxies with argument/result
copying, error sanitation, async revocation and adversarial lifecycle tests. Add
validated bounded IPC and sender/frame authorization before public preload and
contextBridge can be advertised. Snapshot objects themselves do not grant a
capability or validate a live document generation. Windows/Linux, full sandbox
review, nondefault COI and the visible desktop gate remain outstanding.
