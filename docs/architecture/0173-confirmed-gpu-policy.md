# ADR 0173: Confirmed GPU policy and managed compositor exit evidence

## Decision

Expose Electron-shaped app GPU methods using native-confirmed information. A pure
C++ policy tracks early CPU requests, native initialization sealing and observed
upstream ForceCPUPainting. The LibJS host returns bounded snapshots; the maintained
ESM facade owns validation and fresh public objects. Mite stays independent of GUI
services, and Ladybird remains unchanged.

Known CPU configuration maps only compositor/rasterization to disabled_software.
Default GPU capability fields remain absent. The existing Vulkan-specific identity
callback cannot identify Metal or prove restart readiness; CPU-visible frames also
do not imply CPU rendering. Detailed device/driver queries remain unsupported.

Override the existing protected WebView Application process_did_exit interface in
our subclass, preserving the base implementation. Forward genuine Compositor PID
and wait-status evidence through the existing owner-thread bridge. Never label a
disconnect as a crash or SIGKILL as OOM. Retain confirmed policy after process exit;
do not manufacture recovery information from stale identity callbacks.

## Consequences and evidence

The unchanged Code-OSS GPUProcessMainService accepts unknown fields and consumes
real native-backed CPU policy. Controlled tests cover malformed/lifecycle behavior;
actual native tests verify early policy, default unknown and owned Compositor
termination/replacement without visible UI. Synthetic consumer events remain clearly
separate from genuine helper events. Physical GPU/backend telemetry and broader
process/platform events require additional native implementations and acceptance.

See the [API contract](../api/gpu.md) for exact fields, errors and differences.
