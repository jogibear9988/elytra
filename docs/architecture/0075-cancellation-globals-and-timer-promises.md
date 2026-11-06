# 0075: Cancellation globals and native Promise timer consumers

Build on the native check-phase queue with realm-local cancellation state and real
native timer consumers. Keep Mite independent of LibWeb/Firefly. Use a separately
pinned event-target-shim 6.0.2 for public portable event primitives; retain the
existing readable-stream dependency's 5.0.1 version and notices unchanged.

Maintain controller/reason/composition/timeout policy as ESM under bootstrap/web,
and DOMException values separately. Compile the portable event dependency plus
these sources through the existing reproducible factory pipeline. Record authored
inputs with repository-relative hashes and dependency inputs/notices against the
exact lock. Initial globals import no runtime builtins. Native builds require only
Python integrity checks, not npm or a Node runtime. Copy every embedded portable
bundle's verified notice and manifest into packaged applications/CLI releases.

Keep state in branded WeakMaps. Composite signals retain their sources and sources
hold weak dependent links; mark the dependent graph before public callbacks.
Private abort algorithms are provided only to trusted builtin factories through a
GC-rooted ModuleLoader object. No private hook is published globally. Promise timer
factories use those algorithms so propagation stops and synthetic public events do
not suppress/spoof actual cancellation. Native handles and subscriptions are cleared
on settlement/abort. Timeout signals and ref:false Promise timers do not supply
native liveness on their own.

The [API contract](../api/abort.md) records partial conformance, two measured
intentional Node differences, unverified GC/finalization edges and service options
still unsupported. This does not establish HTTP/TLS/fetch or general native I/O
cancellation. Tests compare real Node/Mite behavior and retain explicit assertions
for differences instead of normalizing them into fake equality. All local checks
are non-GUI; hosted desktop gates cover startup in the shared Firefly runtime.
