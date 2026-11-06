# API reference

Mite and Firefly are early implementations. This reference describes the current
public surface, not full Node.js or Electron compatibility. Native validation is
macOS ARM64 plus the scoped [Linux Mite baseline](../development/linux-build.md).
The [Windows x64 Mite gate](../development/windows-build.md) also passes the
43-suite runtime subset. Windows/Linux desktop support and distributable releases
remain work in progress. Use the compatibility matrices to check individual features.

For tutorials, concepts and migration, start at the [documentation home](../index.md).
The [documentation baseline](../reference/documentation-baseline.md) tracks reference
depth still required alongside the implementation.

## Mite

[Mite runtime API](mite.md) covers entry points, modules, globals and Node-style
built-ins. The [CLI entry](cli.md) defines flags and exit behavior.
Mite runs independently of Firefly, Qt and LibWeb.
The [node:crypto reference](crypto.md) details the supported random/hash APIs,
parameters, errors, resource lifetime and Node differences.
The [node:http reference](http.md) covers client streams, options, errors and the
current connection policy alongside the existing server API.
The [child-process reference](child-process.md) describes native spawn, pipes,
process lifetime and remaining unsupported execution modes.

```js
import {createHash} from 'node:crypto';
console.log(createHash('sha256').update('hello').digest('hex'));
```

## Firefly

[Firefly desktop API](firefly.md) covers app lifecycle, native BrowserWindow,
webContents, events and errors. Firefly embeds Mite in its main process and uses
separate Ladybird renderer processes. Detailed entries: [app](app.md),
[BrowserWindow](browser-window.md), [webContents](web-contents.md) and
[preload/contextBridge](context-bridge.md).

```js
import {app, BrowserWindow} from 'firefly';
await app.whenReady();
const window = new BrowserWindow({width: 1000, height: 700});
await window.loadFile('index.html');
app.on('window-all-closed', () => app.quit());
```

## Compatibility and examples

- [Getting started](../getting-started.md)
- [Mite compatibility](../compatibility/mite-0.1.md)
- [Firefly compatibility](../compatibility/firefly-0.1.md)
- [Measured npm packages](../compatibility/npm-packages-0.1.md)
- [Implementation progress and remaining requirements](../PROGRESS.md)

Absent methods fail with ordinary JavaScript API/import errors. Implemented APIs
reject unsupported options rather than returning success placeholders. Error
messages and stack formatting are not generally identical to Node or Electron.
