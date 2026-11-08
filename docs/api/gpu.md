# GPU policy and compositor lifecycle

Firefly implements a partial main-process GPU API using the actual Ladybird painting
configuration. It does not expose Chromium's GPU feature detector or invent device
capabilities. Import `app` from `firefly`, or from `electron` with `--electron-compat`.
Mite alone has no desktop GPU service.

## app.getGPUFeatureStatus()

Returns a fresh plain object. Before native profile initialization it is `{}`.
With the default GPU policy it also remains `{}`: the active backend and fallback
are not yet observable through this adapter. Readiness alone is not GPU evidence.

After native initialization positively selects `ForceCPUPainting::Yes`, the result is:

```js
{
  gpu_compositing: 'disabled_software',
  rasterization: 'disabled_software'
}
```

These fields describe the selected compositor painting policy. Other fields, including
WebGL, canvas, video decode, device identity, driver versions and Graphite, are omitted.
A missing field means unknown information, not a disabled or enabled feature. Changing
a returned object cannot change the engine configuration or subsequent snapshots.

The adapter reads the actual upstream `web_content_options().force_cpu_painting`
after initialization. It does not infer GPU state from absent device identity or
CPU-visible frame buffers: Metal can be active without the Vulkan-specific identity,
and GPU rendering can still return CPU-visible frames.

## app.disableHardwareAcceleration()

Call before readiness/native profile initialization. Returns undefined and selects
real CPU painting in the native startup arguments before helpers start. Repeated
early calls are harmless. Merely requesting the policy does not report it as already
observed; native initialization must confirm it, and startup fails if it was ignored.

```js
import { app } from 'firefly';

app.disableHardwareAcceleration();
app.on('gpu-info-update', () => console.log(app.getGPUFeatureStatus()));
await app.whenReady();
```

The existing `--force-cpu-painting` launch flag selects the same native policy.
`app.commandLine.appendSwitch('force-cpu-painting')` remains launch-only; this method
provides the supported early programmatic path. Changes after the native policy is
sealed reject with `ERR_FIREFLY_GPU_TOO_LATE`, even if app readiness has not yet been
published. Calls after application exit reject with `ERR_FIREFLY_APP_EXITED`.
This initial mapping controls application painting. Media/compute device policy is
outside this subset; it does not disable the operating system's GPU.

## app.getGPUInfo(infoType)

Accepts `basic` or `complete` and returns a rejected Promise with
`ERR_FIREFLY_UNSUPPORTED_API`. Detailed native backend/device telemetry is not
implemented. Invalid types reject with `ERR_INVALID_ARG_VALUE`; no device data is
fabricated from environment variables or Chromium-shaped defaults.

## Events

`gpu-info-update` has no arguments. The initial unknown-to-confirmed CPU policy
transition is published after `ready`. Unchanged policy does not manufacture new
updates; default unknown GPU status does not emit a fake initialization event.

`child-process-gone(event, details)` currently forwards actual Ladybird Compositor
termination only:

| Detail | Meaning |
| --- | --- |
| `type` | `GPU`, mapping Electron's process role to Ladybird's Compositor, including software mode. |
| `serviceName` | `Ladybird Compositor`. |
| `pid` | The actual managed child PID. |
| `reason` | `clean-exit`, `abnormal-exit`, `killed`, `crashed`, or explicitly `unknown` when wait status is unavailable. |
| `exitCode` | POSIX normal exit code, or `128 + signal`; -1 when unknown. |

The event uses the real process monitor's wait status, not an IPC disconnect guess.
Fatal crash signals map to `crashed`; SIGKILL maps to `killed` and does not imply OOM.
Unknown reason and normalized signal codes are intentional differences from complete
Electron diagnostics. Other helper/process roles are not yet forwarded by this API.

Upstream cleanup and compositor replacement still run. Confirmed painting policy is
retained after a child exit, while Code-OSS's GPU service independently clears its
cached status. Recovery does not manufacture a new GPU information event. The existing
upstream identity callback can retain an old identity and cannot prove new readiness;
full recovery telemetry requires additional native evidence.

## Errors and validation

Unavailable native bindings fail explicitly. Malformed JSON, unexpected fields or
unconfirmed enabled values fail with `ERR_FIREFLY_GPU_DATA`. Limits and strict validation
apply to the private status channel; a forged snapshot is never accepted as a native
hardware observation.

[Electron app](https://www.electronjs.org/docs/latest/api/app#appgetgpufeaturestatus)
and [GPUFeatureStatus](https://www.electronjs.org/docs/latest/api/structures/gpu-feature-status)
provide the method names and status vocabulary. Firefly's smaller field set,
unknown default state, limited events and explicit unsupported device queries are
intentional, documented compatibility limits.

Tests run the maintained policy under actual Node/Mite; native policy sealing and
owned-child wait statuses; genuine public early-disable/default-unknown behavior;
and the unchanged pinned Code-OSS GPUProcessMainService using the real Firefly app.
Synthetic consumer events test that service's cache/disposal logic and are labelled
separately. A real offscreen test verifies direct-child Compositor ownership twice,
terminates that child once, checks the genuine PID/reason/exit code, and observes a
replacement with actual CPU arguments. It does not establish physical GPU rendering,
all-platform lifecycle acceptance, a VS Code workbench or workspace open/edit/save.
