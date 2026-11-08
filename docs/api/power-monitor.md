# powerMonitor

Status: **partial macOS read-only queries and passive observations**. Genuine
native idle, thermal and power-source queries, capability checks and passive
observer start/stop pass a local macOS probe. Production JavaScript policy passes
in Node/Mite with a controlled peer. No actual suspend, resume, unplug, screen
lock, thermal change or shutdown transition was induced or accepted by these
tests. Linux/Windows providers are unavailable.

`powerMonitor` is a main-process EventEmitter exported by `firefly`, including
through the explicit Electron compatibility alias. It is absent from the ordinary
renderer capability surface and standalone Mite.

```js
import { app, powerMonitor } from 'firefly';

app.whenReady().then(() => {
  console.log(powerMonitor.getSystemIdleTime());
  console.log(powerMonitor.getSystemIdleState(60));
  console.log(powerMonitor.getCurrentThermalState());
});
```

These APIs observe state; they do not suspend the computer, block sleep, change
power settings, lock the screen or synthesize user activity. `powerSaveBlocker`
is a separate, unavailable service.

## Queries

Valid queries require app readiness. They fail after application exit. Native
results are checked again after crossing the host boundary; an exit during a
query cannot return stale data.

| Method / property | Current contract |
| --- | --- |
| `getSystemIdleTime()` | Nonnegative safe integer, in whole seconds since native combined-session input. |
| `getSystemIdleState(threshold)` | `active`, `idle`, `locked` or `unknown`. Threshold is an integer number of seconds in 1..2147483647. |
| `getCurrentThermalState()` | `unknown`, `nominal`, `fair`, `serious` or `critical`, from native thermal state. |
| `isOnBatteryPower()` | Strict boolean from a known native battery/AC source. Unavailable information fails explicitly. |
| `onBatteryPower` | Readonly property performing the same fresh battery query. |

Queries do not start event observation. Malformed native snapshots fail rather
than becoming plausible default values.

On macOS, idle-state classification is conservative: if reliable lock/session
information is unavailable, the result is `unknown`, even when idle time can be
read. Passive lock observations may supply additional evidence after monitoring
starts. An unavailable or indeterminate power source is not silently treated as
AC. Missing CPU-limit information does not become a fabricated 100-percent value,
and unknown thermal data does not become `nominal`.

## Event registration and availability

Known event names are listed below, but registration also requires that the native
provider advertises that event. The host capability snapshot is fixed at first
use. Battery-change events are advertised only when a real power-source query is
available; CPU speed-limit events require an actual published native limit.

| Event | Arguments |
| --- | --- |
| `suspend`, `resume` | None. |
| `on-ac`, `on-battery` | None. |
| `thermal-state-change` | One `{ state }` object using the thermal enum above. |
| `speed-limit-change` | One `{ limit }` object; integer CPU limit percentage in 0..100. |
| `lock-screen`, `unlock-screen` | None. |
| `user-did-become-active`, `user-did-resign-active` | None; native login-session observations. |
| `shutdown` | One passive event object; see below. |

An unavailable known event or unknown event name rejects with
`ERR_FIREFLY_UNSUPPORTED_API`; listener registration does not pretend that an
observer exists. Applications should handle this refusal when subscribing to
optional events. Native errors reading capability information are preserved.

The supported registration entrypoints are `on`, `addListener`, `once`,
`prependListener` and `prependOnceListener`. Normal listener removal through
`removeListener`, `off` and `removeAllListeners` participates in observer cleanup.
This policy wraps the shared singleton; it is not a blanket claim about every
Electron EventEmitter integration behavior.

`newListener`, `removeListener`, `error` and the EventEmitter `errorMonitor` symbol
are allowed metadata/error listeners and do not start native observation. They do
not provide shutdown cancellation or manufacture OS events. Other ordinary
EventEmitter inspection/configuration methods retain their existing behavior.

## Readiness and observer lifetime

Supported event subscriptions may be registered before app readiness. They can
read the native capability snapshot, but observation is deferred until readiness.
The first native-event listener after readiness starts one owned observer epoch;
additional listeners share it. Removing the last native listener stops that epoch.
A later subscription receives a fresh monotonically increasing epoch.

Removing all ordinary/meta listeners does not remove the internal lifecycle
mechanism. Once-only listeners fire normally and retire observation when they
were the last native listener. Stale epoch callbacks cannot deliver to a new
subscription, and an old stop request cannot stop a newer native epoch.

A startup failure rolls back native listeners instead of leaving inert
subscriptions; metadata listeners remain. Reentrant start/stop/listener changes
are reconciled, including application exit during a native boundary. App exit
revokes the current epoch, stops the observer and clears native listeners. Future
queries and registrations fail; late native packets are ignored.

Epoch IDs are positive integers bounded by signed `INT_MAX`. Pathological
start/stop reentry is capped at 32 reconciliation steps and fails explicitly.
Native JSON snapshots are bounded to 16,384 JavaScript string code units and
validated for their expected value/capability shape. Known events outside the
sealed native capability set are dropped at the native host; invalid current-epoch
packets at the JavaScript boundary fail explicitly instead of producing false
state changes.

## Passive shutdown difference

Firefly observes macOS `NSWorkspaceWillPowerOffNotification`. Its `shutdown`
event is passive:

```js
powerMonitor.on('shutdown', (event) => {
  console.log(event.defaultPrevented); // false
  // event.preventDefault() throws ERR_FIREFLY_UNSUPPORTED_API.
});
```

The event object is frozen. Calling `preventDefault()` fails explicitly and never
claims that shutdown was delayed. There is no OS veto, inhibition or guaranteed
cleanup interval in this slice.

This differs from [Electron powerMonitor](https://www.electronjs.org/docs/latest/api/power-monitor),
whose shutdown handler can attempt to delay system shutdown with
`preventDefault()`. Applications requiring that contract need additional native
integration; subscribing to this passive event does not supply it.

## Errors and unsupported platforms

| Error code | Meaning |
| --- | --- |
| `ERR_FIREFLY_NOT_READY` | A valid query preceded app readiness. |
| `ERR_FIREFLY_APP_EXITED` | Query/registration after exit or exit during a native boundary. |
| `ERR_INVALID_ARG_TYPE` | Invalid receiver/listener, or a non-integer idle threshold. |
| `ERR_OUT_OF_RANGE` | Idle threshold is not positive or exceeds signed `INT_MAX`. |
| `ERR_FIREFLY_UNSUPPORTED_API` | Unknown/unavailable event or shutdown cancellation requested. |
| `ERR_FIREFLY_POWER_UNAVAILABLE` | The native platform/query/observation service is unavailable. |
| `ERR_FIREFLY_POWER_MONITOR_DATA` | Malformed native JSON, capabilities, query value or current-epoch packet. |
| `ERR_FIREFLY_POWER_MONITOR_LIMIT` | Epoch range exhausted or invalid native epoch admission. |
| `ERR_FIREFLY_POWER_MONITOR_REENTRY` | Observer transitions failed to settle within the reconciliation bound. |

Native query/start failures preserve their explicit errors. There is no fallback
that reports AC, a nominal thermal state or a full CPU speed limit after failure.
Linux/Windows providers expose no native event capabilities and fail queries;
native platform acceptance is still required.

## Validation evidence and remaining work

- [Policy fixture](../../firefly/tests/power-monitor/policy.mjs): actual Node/Mite
  run the production API with a controlled native peer. Query validation,
  capabilities, listener aliases, epoch ownership, malformed packets, passive
  shutdown, failure rollback, removal/re-subscription and reentrant disposal pass.
  It does not change or observe actual OS power state.
- [Native provider probe](../../firefly/tests/power-monitor/ProviderProbe.mm):
  genuine read-only values are checked against native framework observations;
  passive observers are started and stopped. No sleep, locks, input or preferences
  are changed.
- [Native host fixture](../../firefly/tests/power-monitor/HostProbe.cpp) and
  [real-OS component fixture](../../firefly/tests/power-monitor/native-queries.mjs)
  pass dispatcher and factory/host/provider wiring respectively. The controlled
  host checks native validation, workers, epochs and late teardown. The real-OS
  component uses a minimal ready context, genuine queries, passive subscriptions
  across two epochs and actual GC. It does not establish desktop lifecycle or
  physical-transition delivery.
- Electron documentation/source informed API shape and the shutdown distinction.
  No actual Electron power query or physical-transition comparison was needed or
  run for this slice.

Real suspend/resume, lock/session changes, battery transitions, CPU/thermal events
and shutdown timing still need opt-in platform acceptance. Shutdown inhibition,
power-setting changes and native Linux/Windows implementations remain planned.
