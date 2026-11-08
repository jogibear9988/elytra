# powerSaveBlocker

Status: **partial macOS implementation; native assertion lifecycle accepted locally**.
The JavaScript facade delegates to a native IOKit assertion controller. A separate
opt-in native probe passes actual assertion creation, aggregation, promotion,
demotion, stop and shutdown, reading back this process's OS records and confirming
no matching assertion remains. This does not establish actual idle/display sleep,
forced sleep, lid-close or physical power behavior. Linux and Windows providers
are unavailable.

`powerSaveBlocker` is a main-process singleton exported by `firefly`, including
through the explicit Electron compatibility alias. It is absent from ordinary
renderer capabilities and standalone Mite. Unlike `powerMonitor` queries, these
methods can be called before `app.whenReady()` resolves.

```js
import { powerSaveBlocker } from 'firefly';

const id = powerSaveBlocker.start('prevent-app-suspension');
try {
  await performBackgroundWork();
} finally {
  powerSaveBlocker.stop(id);
}
```

Starting a blocker deliberately changes OS sleep eligibility. Applications should
hold requests only for the work that requires them.

## Methods

| Method | Contract |
| --- | --- |
| `start(type)` | Returns a nonnegative signed-int32 request ID only after successful native admission. Zero is valid. |
| `stop(id)` | Returns whether the native request was stopped. Unknown IDs return `false`. |
| `isStarted(id)` | Returns whether the native controller currently owns that logical request. Unknown IDs return `false`. |

The two supported types follow the [Electron API](https://www.electronjs.org/docs/latest/api/power-save-blocker):

- `prevent-app-suspension` keeps the system active while allowing display sleep.
- `prevent-display-sleep` also inhibits display sleep and takes precedence while
  at least one such request remains.

Stopping the last display request restores the weaker assertion when application
suspension requests remain. Native code owns this precedence and its registry;
the JavaScript facade does not invent successful requests or cache their state.
IDs are monotonic within a native host. A normal Firefly application has one host;
this is not a process-global ID allocator shared across independent embedded hosts.

`stop` and `isStarted` accept signed-int32 integers, including negative unknown
IDs. Strings, fractions, nonfinite numbers, negative zero and values outside
−2147483648..2147483647 are rejected without native mutation. Methods require
their original singleton receiver. Firefly exposes explicit argument error codes;
they do not promise identical Electron error wording or error classes.

## Native ownership and failure handling

The macOS controller admits at most 64 logical requests. Promotion and demotion
create the replacement assertion before releasing the prior assertion. Failed
transitions preserve existing logical requests; rollback can temporarily retain
one additional assertion pending release. Pending cleanup is retried before
admitting further mutations. Native operations are synchronous and may block;
Electron's wake-lock backend has different scheduling.

Creation, release and quota failures throw `ERR_FIREFLY_POWER_SAVE_CREATE`,
`ERR_FIREFLY_POWER_SAVE_RELEASE` or `ERR_FIREFLY_POWER_SAVE_LIMIT`. An unavailable
provider throws `ERR_FIREFLY_POWER_SAVE_UNAVAILABLE` from `start`; unknown-ID
queries and stops still return `false`. Missing native bridge methods fail with
`ERR_FIREFLY_UNSUPPORTED_API`, rather than simulated success.

Application shutdown permanently revokes this facade, clears native logical
ownership and attempts bounded native release cleanup. Cleanup failures cannot
prevent the rest of application teardown. A native return observed after a
reentrant exit cannot become a successful public result; a late start receives
another best-effort cleanup. Calls after exit throw `ERR_FIREFLY_APP_EXITED`.

Malformed native IDs or nonboolean stop/query results throw
`ERR_FIREFLY_POWER_SAVE_DATA`, permanently revoke the facade, and request native
shutdown. This fails closed instead of trusting an unknown ownership snapshot.
OS release failures can still leave an assertion pending best-effort cleanup;
revocation is not a guarantee that an OS release succeeded.

## Validation scope

The maintained controlled-peer fixture is
`firefly/tests/power-save-blocker/policy.mjs`, run by
`tooling/tests/test_power_save_blocker_policy.py`, passes in actual Node and Mite.
It checks public validation,
native ownership/results, unsupported methods, error identity, malformed
snapshots, reentrant exit and cleanup without requesting OS assertions. The [exact native Host fixture](../../firefly/tests/power-save-blocker/HostProbe.cpp)
passes real Mite factory/binding validation, Unicode errors, forced GC and controller/
VM teardown with a controlled provider. The [exact macOS provider fixture](../../firefly/tests/power-save-blocker/ProviderProbe.mm)
passes precedence, transaction failures, bounded retained ownership and cleanup with
executable-owned IOPM symbols; the original OS assertion functions are never called.
The [unsupported provider fixture](../../firefly/tests/power-save-blocker/UnsupportedProbe.cpp)
passes in a local standalone build; this is not Windows/Linux platform acceptance.
Runtime results are recorded in [progress](../PROGRESS.md). These unit checks do not establish physical sleep
prevention or Electron runtime parity. No Electron power assertion oracle is run.

The opt-in [native lifecycle probe](../../firefly/tests/power-save-blocker/NativeProbe.mm)
passes actual macOS assertion create/readback/aggregate/promote/demote/stop/shutdown.
It briefly changes only this process's owned idle-sleep assertions and confirms
cleanup. It does not trigger sleep, lock, input, preference changes or GUI actions.
Actual display-versus-system sleep behavior, forced sleep, failure recovery against
the OS, and Linux/Windows implementations remain separate acceptance work.

The actual pinned Code-OSS local-desktop/NLS ready diagnostic passes this export
and next fails linking `Electron dialog`. Entry/app-ready observation is not workbench or
open/edit/save acceptance. See [VS Code status](../compatibility/vscode.md).
