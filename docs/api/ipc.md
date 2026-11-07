# ipcMain and ipcRenderer

Status: partial, validated on native macOS ARM64. Firefly implements asynchronous
`ipcRenderer.invoke` with `ipcMain.handle`, `handleOnce` and `removeHandler`. This
is request/reply IPC between separate Mite and Ladybird processes. One-way events,
synchronous IPC, MessagePorts, subframe IPC and full structured clone are not yet
implemented. [macOS ARM64 Alpha 2](https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.2)
includes this subset at `19ad113`; Alpha 1 predates it. The relocated experimental
Linux X11 application also passes a scoped invoke/preload fixture at that source.

The API follows Electron's [ipcMain](https://www.electronjs.org/docs/latest/api/ipc-main)
and [ipcRenderer](https://www.electronjs.org/docs/latest/api/ipc-renderer) contracts
for the tested subset. Error formatting, type support and limits differ below.

## Main-process handlers

```js
import { ipcMain } from 'firefly';

ipcMain.handle('settings:theme', (event) => {
  if (event.sender !== settingsWindow.webContents ||
      event.senderFrame.url !== allowedSettingsURL) {
    throw Error('Sender is not allowed');
  }
  return { theme: 'dark' };
});
```

| Method | Parameters and behavior | Return |
| --- | --- | --- |
| `handle(channel, listener)` | String channel, callable listener; one handler per channel; duplicate registration throws | `undefined` |
| `handleOnce(channel, listener)` | Remove registration before calling the first handler, including when it throws or rejects | `undefined` |
| `removeHandler(channel)` | Remove an existing handler, or do nothing if none is registered; does not cancel an already running handler | `undefined` |

The listener receives `(event, ...args)`. A returned value or the eventual value of
a Promise is copied back to the caller. Throwing/rejection, no matching handler or
an unsupported return value rejects the invocation. Exceptions cross as a bounded
message in a new destination-owned Error; custom properties, source prototypes and
stack traces do not cross. Handle authorization before performing privileged work.

`event.sender` is the actual owning `webContents`, selected by the native routed
page rather than caller arguments. `event.senderFrame` supplies its current native
URL and `isDestroyed()`. `webContents.mainFrame` returns that same limited snapshot
after the first handled invocation, and `null` before one or after invalidation.
This is not the full Electron WebFrameMain API. A document/realm replacement
invalidates the snapshot; same-document requests refresh its URL from native state.
The native bridge admits the bounded request immediately, then orders sender-URL
sampling behind already queued canonical session-history steps. An immediate
invoke after history.replaceState therefore observes the updated native URL.
The queue step revalidates document/realm/window ownership and completes before
application handlers run; navigation or host teardown cannot keep a stale
queued callback alive. No caller-supplied URL is used as authority.
Validate each request's sender and URL; never trust a caller-provided window ID.

## Isolated preload

Author a narrow ESM preload and bundle it using the [preload build step](context-bridge.md):

```js
import { contextBridge, ipcRenderer } from 'firefly';

contextBridge.exposeInMainWorld('settings', {
  getTheme: () => ipcRenderer.invoke('settings:theme')
});
```

`invoke(channel, ...args)` returns a Promise. It is available only from the
configured top-level preload's `require('firefly')` payload, including during
initial preload execution before the first page script. The page can call
`await settings.getTheme()` but receives no raw `ipcRenderer`, Node globals,
filesystem functions or channel selector. Publishing the native invoke function
or the whole ipcRenderer object through contextBridge is rejected. Deliberately
wrapping arbitrary channel names would still give your page arbitrary channels;
use fixed application actions instead.

The isolated preload and the page remain separate worlds. contextBridge's frozen
publication/argument rules still apply to page calls, independently of IPC's
mutable data copies inside the preload and main process.

## Data and resource limits

Supported values: undefined, null, booleans, numbers (including NaN, infinities and
negative zero), UTF-16 strings, plain objects and arrays. Copies preserve cycles,
shared references and array holes. Receiving code gets its own mutable objects;
mutating one does not change the sender's values.

Functions, accessors, proxies, symbols, host/DOM objects, typed arrays, Buffer,
Date, Map/Set, transferable objects and other special classes are unsupported.
Capture rejects getters/proxies without executing their traps. Return a small
plain-data representation when using those types.

| Resource | Current limit |
| --- | --- |
| Channel | String, at most 256 UTF-16 code units |
| Arguments | At most 64 after the channel |
| Graph | 4096 values, 4096 properties, depth 64, array length 4096 |
| All graph strings/keys | 65536 UTF-16 code units |
| Encoded request or reply | 128 KiB |
| Pending requests | 16 per endpoint, 64 per renderer process and 64 across the native host |
| Request deadline | 30 seconds; intentionally bounded alpha policy |

Invalid arguments reject with `ERR_INVALID_ARG_TYPE`; unsupported graphs reject
with `ERR_FIREFLY_IPC_SERIALIZATION`. Local request exhaustion rejects with
`ERR_FIREFLY_IPC_LIMIT`. Remote handler/result errors, host exhaustion and timeouts
currently use `ERR_FIREFLY_IPC_REMOTE`; error text is not Electron-compatible.
Detached/inactive callers reject with `ERR_FIREFLY_IPC_REVOKED` when still callable.

Navigation, renderer loss or window destruction discards pending replies and
reclaims reservations. Application handler side effects already started are not
rolled back. Old-document continuations must not execute in the replacement
document. A deadline cannot interrupt a synchronously blocked JS thread.

## Validation and remaining work

`check-firefly` runs real Electron/Firefly fixtures for early invocation, values,
async ordering, one-time/missing/error handlers, parallel replies and native sender
ownership. Firefly-specific gates cover trap-free rejection, blocked raw capability
publication, per-endpoint limits, navigation/closure cleanup and cross-process host
budget exhaustion/recovery. The existing wire/isolation gates validate malformed
bounded graphs and detached-world/GC behavior. This is not a complete security audit.

`send`, `sendSync`, `postMessage`, `webContents.send`, renderer event listeners,
frame/port routing and Electron's full structured-clone type set remain unsupported.
Missing methods fail as ordinary JavaScript API errors. The inherited main-process
EventEmitter methods do not establish one-way IPC support. Windows/Linux desktop
support requires separate native implementations and acceptance.
