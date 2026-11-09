# webFrame

Availability: isolated top-level Firefly preload; partial native implementation,
validated locally on macOS Release. Fresh Linux/Windows acceptance remains open.
The page itself and main process receive no webFrame binding.

```js
// Authored preload.mjs; bundle to CommonJS before launching.
import { contextBridge, webFrame } from 'firefly';
webFrame.setZoomLevel(0);
contextBridge.exposeInMainWorld('view', {
  setZoom: level => webFrame.setZoomLevel(level)
});
```

Only the four methods below are implemented. Each is bound to the original active
preload document, including when called through an application-authored bridge
wrapper. Raw native methods cannot be published through contextBridge; the marked
webFrame module itself is redacted to empty data. Publish narrow wrappers instead.
The preload import also works through the explicitly enabled Electron alias.

| Method | Result |
| --- | --- |
| `setZoomFactor(factor)` | Set browser zoom synchronously; return `undefined` |
| `getZoomFactor()` | Read the actual native browser zoom factor (`1` is 100%) |
| `setZoomLevel(level)` | Set factor `1.2 ** level`; `0` is 100%, `2` is 144% |
| `getZoomLevel()` | Read `log(factor) / log(1.2)` from actual native state |

Setters require finite numeric primitives. Objects, strings and missing arguments
throw `TypeError` without numeric coercion hooks; nonfinite or out-of-range values
throw `RangeError`. Firefly currently accepts factors from **0.3 through 5**,
matching the pinned Ladybird view's native range. Levels must map into that range;
overflow/underflow is refused. Unsupported webFrame methods/properties are absent.

This changes actual browser viewport/layout, devicePixelRatio, native input scaling
and compositor state. It uses Ladybird's existing parent-view zoom handshake rather
than a CSS transform or a JavaScript-only stored preference. The handshake can run
a nested engine event loop; the adapter retains owner/document and rechecks their
liveness on return. Stale, detached, foreign-realm or nonexecuting source calls fail
with `TypeError`. An unresponsive-parent handshake timeout has not been established.

## Differences and limits

[Electron's webFrame API](https://www.electronjs.org/docs/latest/api/web-frame)
is the naming and level-conversion reference. Firefly currently scopes zoom to the
native view/window rather than propagating a same-origin preference across every
window. Reload follows the existing view behavior; a preload can explicitly set
its desired level for each document, as Code-OSS does. [Main-process webContents zoom methods](web-contents.md#browser-zoom) share the same
native state. Persistent origin preferences, frame traversal, visual zoom limits, text/CSS
insertion, JavaScript execution and spellcheck are not supplied by this slice.

## Validation

`check-firefly-isolation` includes direct real-engine numeric/NaN/infinity/object/
coercion checks, frozen module shape and native publication denial without GUI.
`check-firefly-preload-zoom` runs a hidden production window twice, measuring actual
viewport width and devicePixelRatio at early preload level 2, factors 1/2/0.75/1.2,
negative level and reset, with Firefly-only 0.3/5 boundaries.
The configuration IPC Promise also calls the setter from a protected source job. It checks invalid values, module redaction, raw method
refusal and page isolation. A second same-origin Firefly window records unexpected
resize/zoom changes. Actual Electron 45.2.3 passes the supported numeric/layout/
reload subset; Firefly-specific validation, module redaction and window scope are
separate assertions. No Code-OSS workbench launch follows from this API test.
