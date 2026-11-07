# screen

Read the desktop layout and cursor position from the main process.

**Availability:** partial macOS implementation. The native provider compiles and
its actual OS snapshots pass a windowless CLI probe on macOS ARM64. Geometry,
readiness and event policy pass through the real API factory in Node and Mite.
The public Firefly/Electron desktop fixture also passes in hosted macOS
[run 42316853522](https://github.com/jogibear9988/elytra/actions/runs/42316853522)
at `19ad113`. Linux X11 native queries and owner-scoped notification wiring are
implemented; their hosted provider/public-app acceptance is pending. Native
Windows display support and physical hotplug/DPI-change acceptance remain open.

```js
import {app, screen} from 'firefly';
await app.whenReady();
console.log(screen.getPrimaryDisplay().workArea);
console.log(screen.getCursorScreenPoint());
```

`screen` is an EventEmitter singleton. CommonJS `require('firefly').screen`
refers to the same object as the initial ESM export. It is unavailable in
standalone Mite, preloads and document JavaScript. The browser's `window.screen`
is a separate DOM object. No renderer privilege is added by this service.

## Methods

All five query methods are synchronous and require app readiness. Calling them
before `app.whenReady()` rejects synchronously with `ERR_FIREFLY_NOT_READY`.
Each call reads a fresh native snapshot. Returned display objects have independent
nested data; mutating one does not change later queries or event comparison.

| Method | Parameters | Return |
| --- | --- | --- |
| `getAllDisplays()` | none | `Display[]`; empty if no display is available |
| `getPrimaryDisplay()` | none | `Display` for the menu-bar display |
| `getCursorScreenPoint()` | none | `{x, y}` absolute cursor position in DIP |
| `getDisplayNearestPoint(point)` | `{x, y}` signed 32-bit integers | `Display` with minimum distance to its bounds |
| `getDisplayMatching(rect)` | `{x, y, width, height}` signed 32-bit integers; dimensions nonnegative | `Display` with greatest intersection area |

Coordinates are device-independent pixels (DIP), with the primary display's top
left at `(0, 0)` and positive Y downward. Other displays can have negative origins.
No cursor movement or capture occurs. AppKit coordinates are converted at the
native boundary, including the visible work area excluding dock/menu-bar space.

Nearest-distance and largest-intersection ties prefer the primary display, then
native enumeration order. When no intersection has positive area, matching uses
the rectangle's center and nearest-distance policy. This is the current Firefly
policy; all Electron tie and degenerate-rectangle behavior is not established.
Invalid coordinate types/fractions/overflow throw `ERR_INVALID_ARG_TYPE`;
negative dimensions throw `ERR_OUT_OF_RANGE`. With no primary display, all queries
except `getAllDisplays()` throw `ERR_FIREFLY_DISPLAY_UNAVAILABLE` on macOS.
Linux provider failures (including missing/ambiguous RandR mappings) throw that
error from any query, including `getAllDisplays()`. This is an explicit partial
platform difference; no synthetic display is substituted.

## Display record

| Field | Meaning |
| --- | --- |
| `id` | CoreGraphics display ID on macOS; native RandR output XID on Linux; neither is an array index or persistent hardware identity |
| `bounds`, `workArea` | `{x, y, width, height}` integer DIP rectangles |
| `size`, `workAreaSize` | `{width, height}` from the corresponding rectangles |
| `scaleFactor` | AppKit backing scale or Qt native device scale |
| `rotation` | Native clockwise rotation in degrees |
| `internal` | CoreGraphics built-in flag on macOS; omitted on Linux pending verified panel metadata |
| `detected` | true for a currently enumerated display |
| `label` | AppKit localized name or Qt/X11 output label |
| `displayFrequency` | Native CoreGraphics mode or Qt refresh rate in Hz; unavailable values need platform-specific interpretation |
| `touchSupport`, `accelerometerSupport` | `unknown`; not capability detection |

Color space/depth, HDR and other Electron Display properties are not implemented
and are omitted. A variable-refresh display can report an unavailable frequency.
Mirroring, mixed-DPI layouts, rotation and display switching require wider native
hardware coverage; synthetic geometry tests do not establish those OS behaviors.

## Events

Subscribe before or after readiness:

```js
screen.on('display-metrics-changed', (event, display, changedMetrics) => {
  console.log(display.id, changedMetrics);
});
```

| Event | Callback arguments |
| --- | --- |
| `display-added` | `(event, newDisplay)` |
| `display-removed` | `(event, oldDisplay)` |
| `display-metrics-changed` | `(event, display, changedMetrics)` |

`event` is currently an empty, noncancellable object. Metrics are `bounds`,
`workArea`, `scaleFactor`, `rotation`. The native Qt application queues display
notifications through the existing bridge owner onto the VM thread. Snapshots
coalesce intermediate changes: unchanged observations emit nothing. Removed
displays emit before additions/metric updates; this ordering is Firefly policy,
not a promised Electron ordering. The baseline is committed before listener calls
so reentrant reads do not duplicate events. Shutdown cancels owned notifications.

Actual hotplug, refresh-rate and backing-scale notification completeness still
need native acceptance. Do not use this as a lossless hardware event log.

## Unsupported operations and validation

`screenToDipPoint`, `dipToScreenPoint`, `screenToDipRect` and `dipToScreenRect`
throw `ERR_FIREFLY_UNSUPPORTED_API`. They are not implemented macOS conversions.
Other absent members produce ordinary JavaScript API errors.

The [modern display example](../../examples/screen/main.mjs) prints the layout
without creating a window:

```sh
build/renderer/bin/firefly examples/screen/main.mjs
```

To validate the policy and actual native provider without launching a desktop app:

```sh
cmake --build build/renderer --target firefly-screen-probe --parallel 3
python3 tooling/tests/test_screen.py --mite build/release-mite/bin/mite --probe build/renderer/bin/firefly-screen-probe
```

The shared public export fixture lives in
[screen.mjs](../../firefly/tests/migration/screen.mjs) and runs in the existing
batched desktop CI suite. Its results must be distinguished from the windowless
probe. API baseline: [Electron screen](https://www.electronjs.org/docs/latest/api/screen)
and [Display](https://www.electronjs.org/docs/latest/api/structures/display).


## Experimental Linux X11 provider

The provider requires the actual xcb GUI thread and RandR 1.3. Each Qt screen must
map uniquely to an active connected RandR output; failures and reflected layouts
are explicit errors. Bounds/work area/cursor use Qt DIP relative to the primary
origin. No cursor movement occurs. Internal-panel/color/depth fields are omitted;
physical/mixed-DPI, nonprimary work-area fidelity, rotation and hotplug remain
unverified. See [ADR 0095](../architecture/0095-linux-native-display-snapshots.md).

The shared geometry/readiness/event policy passes in Node/Mite without GUI. Hosted
native tests independently compare output IDs, bounds/work area and cursor through
xrandr/EWMH/Xlib. The actual Mite-driven app gate now requires these same native
observations through public screen methods while retaining renderer/kernel checks.
Those new native gates must pass on this implementation before Linux screen is
listed as tested. Watchers use the bridge owner as Qt connection context; broader
physical notification completeness is still pending.
