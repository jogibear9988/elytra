# Create a Firefly desktop app

Firefly currently has a tested native macOS ARM64 development slice. Build it with
the [renderer guide](../development/renderer-build.md). Windows/Linux desktop and
production distribution are not yet available.

## Open the existing app

```sh
python3 tooling/build_hello.py
build/renderer/bin/firefly build/firefly-hello-world/main.mjs
```

This runs Mite for the main entry and opens the local HTML in a separate Ladybird
renderer. Closing the last window requests app quit. The sample sources use ESM for main, preload and page scripts. The build bundles
preload.mjs into the CommonJS payload required by the current sandbox; no Node
runtime is packaged. The example logs the observed renderer PID; it does not rely on a browser-host Node global.

## Main entry

For a small ESM app, save main.mjs beside index.html:

```js
import {app, BrowserWindow} from 'firefly';
await app.whenReady();
const window = new BrowserWindow({width: 1000, height: 700, title: 'My Firefly app'});
await window.loadFile('index.html');
app.on('window-all-closed', () => app.quit());
```

Load failures reject the Promise and must be handled in a real app. Only supported
window options are accepted. Electron's activation/dock/Cmd-Q and beforeunload
behavior are not fully implemented; this simple close policy is explicit.

## Renderer content

A local index.html can load its own renderer.js:

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta http-equiv="Content-Security-Policy" content="default-src 'self'; script-src 'self'">
  <title>My Firefly app</title>
</head>
<body>
  <h1>Hello from the renderer</h1>
  <button id="greet">Say hello</button>
  <p id="message"></p>
  <script src="renderer.js"></script>
</body>
</html>
```

```js
// renderer.js
const button = document.getElementById('greet');
button.addEventListener('click', () => {
  document.getElementById('message').textContent = 'Hello from LibWeb!';
});
```

This uses browser DOM APIs only. process, require, Buffer and Node module imports
are unavailable in the renderer. Choose a CSP appropriate to your actual assets;
a CSP declaration is not a complete security audit or Chromium feature guarantee.

## Main/renderer communication

A bounded [CommonJS preload/contextBridge](../api/context-bridge.md) is supported;
the existing hello-world demonstrates it. [Invoke/handle IPC](../api/ipc.md) adds
bounded native request/reply through fixed preload actions. Validate sender identity
and URL in each main handler before granting native capabilities. Read [security](security.md) and [Electron migration](../migration/from-electron.md).

## Continue with a real app

The [pinned Designer](../../examples/web-component-designer/README.md) builds a real
TypeScript/Monaco application and has a native boot/edit smoke gate. Use the
[app](../api/app.md), [BrowserWindow](../api/browser-window.md) and
[webContents](../api/web-contents.md) entries for actual lifecycle/loading contracts.
