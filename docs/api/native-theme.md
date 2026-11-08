# nativeTheme

Observe macOS appearance preferences and select an application-local light,
dark or system theme.

**Process:** main. **Availability:** partial macOS ARM64 implementation. Native
preference/notification ownership tests and Node/Mite API-factory comparisons
pass without GUI. Targeted Firefly and Electron 45.2.3 acceptance passes for
explicit overrides and CSS queries in existing and newly created hidden windows.
Physical system-theme/accessibility changes, visible native chrome and broader
platform behavior need further acceptance. Windows/Linux desktop support remains
unfinished.

```js
import {app, nativeTheme} from 'firefly';

app.whenReady().then(() => {
  nativeTheme.themeSource = 'dark';
  console.log(nativeTheme.shouldUseDarkColors);
});
nativeTheme.on('updated', () => {
  console.log(nativeTheme.shouldUseDarkColors);
});
```

The singleton is an EventEmitter. CommonJS and the initial ESM export refer to
the same object. It is not exported by standalone Mite, preloads or document JS.
Applications may expose narrowly validated theme actions through their own
[IPC handlers](ipc.md); no automatic renderer privilege is added. Querying or
setting nativeTheme does not require app readiness.

## Properties

| Property | Type | Behavior |
| --- | --- | --- |
| `themeSource` | `system`, `light` or `dark` | Read/write; defaults to system |
| `shouldUseDarkColors` | boolean, read-only | Effective application choice, including override |
| `shouldUseDarkColorsForSystemIntegratedUI` | boolean, read-only | Same effective choice on macOS |
| `shouldUseHighContrastColors` | boolean, read-only | Actual macOS increase-contrast preference |
| `shouldUseInvertedColorScheme` | boolean, read-only | Actual macOS invert-colors preference |
| `prefersReducedTransparency` | boolean, read-only | Actual macOS reduce-transparency preference |
| `shouldDifferentiateWithoutColor` | boolean, read-only | Actual macOS differentiate-without-color preference |

Invalid source values throw `ERR_INVALID_ARG_VALUE`; no string coercion occurs.
Assigning the same source is a no-op. Read-only properties have getter-only
descriptors; assignment in strict JS throws TypeError. Getters query fresh native
system data. A selected light/dark source does not rewrite the OS preference.

### Applying an override

The native host reuses Ladybird's application appearance settings and Qt
SystemAppearance API. It sets interface and website color preferences together,
so existing views receive native color-preference messages and new views inherit
the choice. Changes affect this Firefly application's selected native profile; global
macOS defaults are never written. Setting system removes the application override.
A new main process starts at system; an earlier launch's persisted explicit source
is reset unless the new bootstrap selects an override.

The renderer's `prefers-color-scheme` queries follow the selected source. Tests
check real CSS state, not a DOM class or value echoed by an IPC handler. Hidden
Ladybird views can defer rendering-driven MediaQueryList change notifications;
the shared hidden-window fixture independently polls fresh matchMedia queries.
It does not certify hidden-window change-event timing or visible rendering.

The accessibility properties report native preferences. This does not establish
complete browser forced-colors/high-contrast rendering or every native UI control's
visual styling. The Windows-only inForcedColorsMode property is absent on macOS.

## Event: updated

The event has no arguments. Read properties to discover the new state.

Native OS appearance and accessibility notifications use the existing Bridge
queue. Application overrides also queue a notification. Firefly compares the
last observed snapshot, including themeSource, commits the new baseline before
listener callbacks and suppresses unchanged snapshots. Reentrant reads do not
duplicate events. Multiple native notifications/rapid source transitions can
coalesce; this is not a lossless transition log or an exact Electron event-count
guarantee.

OS dark-state observation reuses Ladybird's SystemAppearance notifications;
accessibility observation uses NSWorkspace notifications. Both are scoped to the
Bridge QObject. Observer removal and a QPointer guard prevent queued native
notifications from calling a destroyed owner. No source-world JS pointers are
retained by native notification blocks.

## Example and validation

The existing [Dark mode example](../examples/dark-mode.md) uses toggle/system
buttons, nativeTheme and isolated invoke/handle IPC. Its individual button/visual
acceptance is still manual; the direct API/CSS fixture has separate measured
coverage.

Windowless validation:

```sh
cmake --build build/renderer --target firefly-theme-probe --parallel 3
python3 tooling/tests/test_theme.py --mite build/release-mite/bin/mite --probe build/renderer/bin/firefly-theme-probe
```

The native probe reads actual OS preferences and posts an in-process accessibility
notification to test registration/teardown, without changing a system setting.
The policy suite covers overrides, system flags, queued/coalesced events,
reentrant reads, read-only/invalid values and native setter failure. The
[public fixture](../../firefly/tests/migration/theme.mjs) runs in the batched
desktop suite with the pinned Electron reference. Its ESM entry finishes module
evaluation before the ready callback, avoiding an Electron startup wait cycle.

Baseline: [Electron nativeTheme](https://www.electronjs.org/docs/latest/api/native-theme).
Full event/coercion/error-shape, physical OS changes, visible native chrome,
accessibility rendering and Windows/Linux acceptance remain open.
