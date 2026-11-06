# Move an Electron application to Firefly

Firefly follows Electron's public API design, with Mite in the main process and
Ladybird in the renderer. Its current macOS ARM64 development slice supports only
a small portion of Electron. The behavioral reference is Electron 45.2.3.

## Identify the boundaries

Inventory main-process imports, BrowserWindow options, preload scripts, renderer
Node use, IPC channels, native modules, web features and distribution tooling.
Check both the [Firefly matrix](../compatibility/firefly-0.1.md) and the
[Mite matrix](../compatibility/mite-0.1.md): main-process Node dependencies also need
to work. Read [process boundaries](../concepts/runtime-and-processes.md).

Bounded isolated preloads/contextBridge and invoke/handle IPC have native acceptance
for the documented subset. Check [IPC limits and sender authorization](../api/ipc.md)
before migrating; event/synchronous/port/subframe IPC remains blocked. Menu, Tray,
dialogs, session, shell, clipboard and auto-updates are also unavailable. Do not
remove security boundaries to force such an application to launch.

## Start with the supported window slice

For an app that needs only supported lifecycle/window/loading APIs, change the
main-process import:

```diff
-import {app, BrowserWindow} from 'electron';
+import {app, BrowserWindow} from 'firefly';
```

CommonJS can similarly use require('firefly'). There is no public electron alias.
Keep supported node: imports unchanged. Launch with the source-built executable:

```sh
build/renderer/bin/firefly path/to/main.mjs
```

The [first desktop app](../guides/first-desktop-app.md) is a complete small example.
Create windows after app.whenReady, await loading, handle rejection and choose an
explicit last-window close policy. Do not assume Electron's macOS activation/dock
behavior is implemented.

## Review every window option

Only width, height, title, show and restricted webPreferences are supported.
nodeIntegration must remain false; contextIsolation and sandbox must remain true.
preload and other options reject. Consult the [BrowserWindow reference](../api/browser-window.md)
before copying an Electron constructor, including apparently harmless options.

Loading accepts file/http/https and only absent or empty options. Query/hash,
headers and post-data options are not implemented. Relative loadFile paths resolve
from the entry module directory. See [webContents](../api/web-contents.md).

## Test the actual web application

The renderer has browser APIs, not Mite globals. Bundle browser dependencies and
check the page under LibWeb rather than treating a Chromium success as sufficient.
Compare layout, input, navigation, workers, fonts and required Web APIs using the
real application. The [Designer sample](../../examples/web-component-designer/README.md)
provides an observed document-edit/Monaco workflow; it is not evidence for every
feature of that app or arbitrary Electron applications.

There is no public executeJavaScript or DevTools API. Plan testing around existing
native gates and application-level bounded checks; read [testing/debugging](../guides/testing-and-debugging.md).

## Reassess distribution

Electron packagers and auto-updaters do not automatically package Firefly. The
experimental [macOS packaging path](../guides/packaging.md) relocates the actual
runtime, helpers, resources and notices. Native Windows/Linux desktop packages,
production signing/notarization and update delivery remain outstanding.

Record the exact source revision, platform, passed application scenarios and
remaining blockers. A changed import alone is not a completed migration.
