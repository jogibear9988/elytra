# API reference

Mite and Firefly are early implementations. This reference describes the current
public surface, not full Node.js or Electron compatibility. Native validation is
macOS ARM64 plus the scoped [Linux Mite baseline](../development/linux-build.md).
The [Windows x64 Mite gate](../development/windows-build.md) and Linux x64
pass all 58 runtime suites including decompression at 84c3b2b. Windows/Linux desktop support and distributable releases
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
The [node:net reference](net.md) covers TCP sockets/listeners, stream ownership,
AbortSignal cancellation and native shutdown.
The [node:tls reference](tls.md) covers native client records, trust, verification,
stream ownership and unfinished server/context modes.
The [node:http reference](http.md) covers client streams, options, errors and the
current connection policy alongside the existing server API.
The [node:https reference](https.md) describes verified native HTTPS clients using
those same HTTP request/response classes.
The [cancellation reference](abort.md) covers EventTarget/AbortController/AbortSignal
and native Promise timer consumers.
The [timers reference](timers.md) covers native timeout/immediate callbacks,
checkpoints and handle lifetime.
The [Web Streams reference](web-streams.md) covers readable/writable/transform
streams, byte ownership, piping, pressure and current interoperability limits.
The [Node/Web stream bridge reference](stream.md) describes conversion, endpoint
ownership, errors and native HTTP integration.
The [Fetch reference](fetch.md) covers Headers, Request, Response, incremental
HTTP/HTTPS bodies, redirects, cancellation and unsupported web-platform modes.
The [Blob/File/FormData reference](blob-formdata.md) covers immutable bytes, file
metadata, multipart uploads and body consumers.
The [decompression reference](decompression.md) describes native Web Stream gzip/
deflate decoding, automatic Fetch content handling, pressure and integrity limits.
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
[preload/contextBridge](context-bridge.md), [ipcMain/ipcRenderer](ipc.md) and
[screen](screen.md), [clipboard/ClipboardItem](clipboard.md) and [nativeTheme](native-theme.md).

```js
import {app, BrowserWindow} from 'firefly';
await app.whenReady();
const window = new BrowserWindow({width: 1000, height: 700});
await window.loadFile('index.html');
app.on('window-all-closed', () => app.quit());
```

## Compatibility and examples

- [Desktop feature examples and API dependencies](../examples/index.md)
- [Getting started](../getting-started.md)
- [Mite compatibility](../compatibility/mite-0.1.md)
- [Firefly compatibility](../compatibility/firefly-0.1.md)
- [Measured npm packages](../compatibility/npm-packages-0.1.md)
- [Implementation progress and remaining requirements](../PROGRESS.md)

Absent methods fail with ordinary JavaScript API/import errors. Implemented APIs
reject unsupported options rather than returning success placeholders. Error
messages and stack formatting are not generally identical to Node or Electron.
