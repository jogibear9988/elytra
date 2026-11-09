# Window and view backgrounds

Firefly supports an opaque application background through BrowserWindow's
backgroundColor constructor option, setBackgroundColor and getBackgroundColor,
and through WebContentsView.setBackgroundColor. The default is white.
These are main-process APIs; webContents has no public color setter/getter.

```js
import { app, BrowserWindow } from 'firefly';

await app.whenReady();
const window = new BrowserWindow({ backgroundColor: '#1E1E1E' });
window.setBackgroundColor('rgb(30, 30, 30)');
console.log(window.getBackgroundColor()); // #1E1E1E
```

## Color values

The supported opaque formats are #RGB, #RRGGBB, #ARGB, #AARRGGBB,
comma-separated rgb/rgba and hsl/hsla, and lowercase named colors from the native
color table. Hex alpha follows Electron's ARGB spelling, not CSS RGBA spelling.
The alpha component must be fully opaque. getBackgroundColor reads native state
and returns uppercase #RRGGBB; it does not report document CSS backgrounds.

Invalid strings fall back to white, following the tested Electron behavior.
The string transparent also falls back to white. Non-string setter values throw
TypeError. Valid nonopaque colors fail with ERR_FIREFLY_UNSUPPORTED_API before
changing native state. Transparent native windows and materials remain unsupported;
the current Qt CPU/Cocoa presentation path is opaque. Full CSS Color 4 parsing is
not claimed; the functional syntax above uses legacy comma-separated components.

## Rendering and lifetime

The constructor supplies the native color before showing the window. Setters
update the real viewport fallback and send bounded, generation-checked native
configuration to the renderer. The value is a canvas base, not an injected style:
authored html/body backgrounds retain precedence. CSS inspection and the page's
own scripts are not changed. Updates cross an asynchronous renderer boundary;
an absolute zero-flash first-frame guarantee is not established.

The policy is scoped to each actual top-level view context. A native
WebContentsView has its own base and retains it when reparented. HTML iframes keep
their ordinary canvas rules. Document replacement and helper handoff receive the
current native configuration; destruction removes the UI registry entry.

Private metadata travels through the existing trusted session-state channel.
It is stripped before preload validation or script execution. It grants no page
capability and uses neither DOM mutation nor renderer-authored IPC to identify a
view. See [ADR 0183](../architecture/0183-native-canvas-backgrounds.md).

The API follows [Electron BrowserWindow colors](https://www.electronjs.org/docs/latest/api/browser-window#winsetbackgroundcolorbackgroundcolor)
within the limits above. Native pixel acceptance, platform scope and the current
Code-OSS startup checkpoint are recorded in [PROGRESS](../PROGRESS.md).
