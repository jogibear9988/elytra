# ipcMain and ipcRenderer

Status: partial, validated on native macOS ARM64. Firefly implements asynchronous
`ipcRenderer.invoke` with `ipcMain.handle`, `handleOnce` and `removeHandler`. This
is request/reply IPC between separate Mite and Ladybird processes. A new local
macOS subset also supports ipcRenderer.send to ipcMain event listeners, described
below. Main-to-renderer webContents.send and isolated-preload listener methods
also have local macOS acceptance. Synchronous IPC, MessagePorts, subframe IPC and
full structured clone remain unfinished. [macOS ARM64 Alpha 2](https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.2)
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
filesystem functions or channel selector. Publishing a native invoke/send function
or the whole ipcRenderer object through contextBridge is rejected. Deliberately
wrapping arbitrary channel names would still give your page arbitrary channels;
use fixed application actions instead.

The isolated preload and the page remain separate worlds. contextBridge's frozen
publication/argument rules still apply to page calls, independently of IPC's
mutable data copies inside the preload and main process.

## ipcRenderer.send(channel, ...args) and ipcMain events

A partial asynchronous renderer-to-main event path is implemented. Use only in the
configured isolated top-level preload. It returns undefined immediately; it has no
application reply Promise. Validation, unsupported data, inactive callers and local
pending exhaustion **throw synchronously**, using the corresponding error codes
below. The data types and budgets are the same as invoke; binary data/ports are
still unavailable. For example, wrap a fixed channel in a narrow preload action:

```js
// main
ipcMain.on('editor:changed', (event, change) => {
  if (event.sender !== editorWindow.webContents ||
      event.senderFrame.url !== allowedEditorURL) {
    throw Error('Sender is not allowed');
  }
  saveAuthorizedChange(change);
});

// isolated preload
contextBridge.exposeInMainWorld('editor', {
  changed: (change) => { ipcRenderer.send('editor:changed', change); }
});
```

Main receives (event, ...args), using the same native sender/senderFrame snapshot
as invoke. Canonical history ordering applies to both: a send immediately following
history.replaceState reports the updated native URL. ipcMain.on/addListener, once,
removeListener/off and removeAllListeners use the existing EventEmitter behavior.
Listeners run in registration order with this === ipcMain; once is removed before
calling it, and all listeners for one event see the same destination-owned mutable
copy. Sender data remains independent. A channel with no listener is ignored;
the default error-channel listener prevents its name alone from throwing, as in
Electron. Application listener exceptions retain normal main-process error behavior.
Listener return values are ignored, including Promises; they are not serialized
or awaited and do not become invoke replies. Application failures are not delivered
back to send callers.

Events and invokes share the 16 endpoint / 64 renderer / 64 native-host outstanding
budgets and monotonic request IDs. A private native acknowledgement releases event
credit after synchronous listener dispatch, or after an ignored event. This ACK is
not exposed to application JavaScript and creates no reply Promise. Native quota
stays reserved through history ordering and VM-busy retries. Removing a document/
window reclaims its slots; dispatch rechecks ownership before any callback and
again before ACK, so a listener may destroy its own window safely.

Local validation/exhaustion throws immediately. A later main admission refusal or
unanswered 30-second event deadline releases local credit and writes an explicit
ERR_FIREFLY_IPC_SEND helper diagnostic. There is no delivery receipt or retry API;
send returning undefined is not proof the application processed a message. This
bounded alpha behavior differs from Electron's unbounded application-facing send
interface. Main independently validates event wire graphs before VM dispatch;
the main factory also rejects invalid argument envelopes with a diagnostic before
EventEmitter dispatch. Neither path treats malformed renderer input as an application
listener exception.

This does not implement event.reply, event.returnValue, webContents ipc-message
listeners. SenderFrame remains the limited document-bound snapshot above. Use
invoke for a supported document-bound application reply; the private ACK itself
still supplies no application reply channel.

## webContents.send(channel, ...args)

Returns undefined. Send a bounded plain-data event to the configured top-level
isolated preload of that webContents. The native preload service announces its
current document/realm endpoint independently of application invoke/send calls.
After the real document has loaded, main can send the first application message.
A missing, unbound or inactive preload throws ERR_FIREFLY_IPC_NOT_READY; this subset
does not queue messages for a future document. A destroyed owner uses the existing
webContents lifecycle errors. Invalid channel/argument counts throw
ERR_INVALID_ARG_TYPE; unsupported graphs/budget overflow use
ERR_FIREFLY_IPC_SERIALIZATION. All are synchronous.

```js
// main, after the owned window has loaded its document
window.webContents.send('editor:theme', {name: 'dark'});

// isolated preload; register before main sends
ipcRenderer.on('editor:theme', (_event, theme) => {
  applyThemeToApplication(theme);
});
```

Messages capture the native current document and realm **at the send call**. Native
routing refuses duplicates/stale document identities and never retargets queued
messages to a replacement document. This is not event.reply: a later explicit
webContents.send call targets the then-current document, rather than the document
that caused an earlier event. Use invoke's document-bound reply where appropriate.
No arbitrary JavaScript execution is added to the page.

Outgoing main events have a separate 16-per-document / 64-per-host outstanding
budget, shared across channels. Exhaustion throws ERR_FIREFLY_IPC_LIMIT. A private
ACK releases credit after synchronous listener dispatch, including an event with
no listener or a throwing listener. ACKs neither serialize listener results nor
wait for returned Promises. The receiver also bounds deferred messages to 16 when
its world is executing another frame; delivery waits for a fresh isolated frame.

If an ACK is missing for 30 seconds, a native timer retires that document's outgoing
endpoint, releases its reservations and logs ERR_FIREFLY_IPC_SEND. Further sends
throw ERR_FIREFLY_IPC_REVOKED until a **new document** is loaded. Credit is not
repeatedly replenished to a stalled peer, which would allow native transport queues
to grow. The deadline cannot interrupt blocked JavaScript or undo messages already
queued at the peer. Navigation/closure also reclaims outstanding state. There is
no application-visible delivery receipt, retry operation or performance guarantee.

## Isolated-preload event listeners

| Method | Contract | Return |
| --- | --- | --- |
| on(channel, listener) | Add in registration order | ipcRenderer |
| addListener(channel, listener) | Alias of on, including function identity | ipcRenderer |
| once(channel, listener) | Remove before calling once | ipcRenderer |
| removeListener(channel, listener) | Remove the latest matching registration; no-op if absent | ipcRenderer |
| off(channel, listener) | Alias of removeListener | ipcRenderer |
| removeAllListeners([channel]) | Remove that channel or every registration | ipcRenderer |

Only callable listeners belonging to the owning isolated realm are accepted.
Invalid channel/function types throw ERR_INVALID_ARG_TYPE; foreign-realm callbacks
throw ERR_FIREFLY_IPC_SERIALIZATION; inactive/revoked endpoints throw
ERR_FIREFLY_IPC_REVOKED. At most 64 registrations per document, across all channels;
adding another throws ERR_FIREFLY_IPC_LIMIT. Removal releases the roots immediately.
The document/session teardown clears listeners and the exported module object.

Listeners receive (event, ...args), with this === ipcRenderer. event.sender is that
same private module object and event.ports is a fresh empty array. No ports are
transferred. The event is a limited plain object, not complete DOM/Event parity.
Do not forward it as an application payload. Raw native listener methods are blocked
from contextBridge publication, just like send/invoke. Use fixed preload handlers
and publish only appropriate application data/actions.

Arguments are copied into the destination realm as mutable data, preserving
cycles/aliases/UTF-16. All listeners for one event share that destination copy;
mutations do not affect main. Dispatch snapshots registrations: removing a listener
during a callback does not remove it from the current snapshot, and additions wait
for the next event. Reentrant document/world revocation stops subsequent callbacks.
Listener errors stop that dispatch, produce ERR_FIREFLY_IPC_LISTENER diagnostics
and release credit. Returned values/Promises are ignored. Unhandled channels,
including error, are ignored by this renderer subset; it is not a full EventEmitter
implementation. Introspection/prepend methods and complete exception/DOM event
reporting remain absent.

These are trusted **preload** listener APIs. General page-function arguments through
contextBridge and Code-OSS's full published callback API remain incomplete; adding
these listeners does not establish that the actual workbench preload can publish
all of its APIs unchanged. Binary values, ports and broader bridge contracts still
need their own end-to-end acceptance.

## Data and resource limits

Supported values: undefined, null, booleans, numbers (including NaN, infinities and
negative zero), UTF-16 strings, plain objects and arrays. Copies preserve cycles,
shared references and array holes. Receiving code gets its own mutable objects;
mutating one does not change the sender's values.

Binary IPC, MessagePorts and efficient disk-backed file/stream access are now
prioritized for the [Code-OSS target](../compatibility/vscode.md). See the
[file transport plan](../guides/file-transfer.md). This does not change the current
supported values or limits below; no zero-copy or File transfer support is claimed.

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
| Pending invokes/events | Shared 16 per endpoint, 64 per renderer process and 64 across the native host |
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

check-firefly-ipc-events adds a batched hidden-window send acceptance gate: early
preload, event/invoke ordering, once, cycles/UTF-16, synchronous trap-free rejection,
raw send publication refusal, native history/sender identity, two-document reload,
16-message credit exhaustion/recovery and reentrant window destruction. The native
main factory/EventEmitter policy also runs without a GUI in Node and actual Mite.
A separate real Electron reference covers the common positive event contract; it
does not establish parity with Firefly's limits or document-snapshot semantics.

```sh
cmake --build build/renderer --target check-firefly-ipc-events --parallel 3
```

Local macOS Release passes. Linux CI requires the gate under Xvfb; fresh hosted
acceptance and native Windows desktop remain open. sendSync, postMessage,
frame/port routing and Electron's full structured-clone type set remain unsupported. Missing methods fail as ordinary
JavaScript API errors. See [ADR 0136](../architecture/0136-renderer-to-main-events.md).

check-firefly-ipc-outbound batches main-to-renderer acceptance without requiring a
first application message from preload: clone/order/once/aliases/removal, strict
validation, listener roots/queue budgets, credit recovery, native sender/document
replacement and listener-driven close. A real owned renderer is OS-verified and
SIGSTOPed, then resumed: a real 30-second ACK timeout retires the endpoint, further
sends fail and a fresh document works. The fixture always reaps its owned process
group. A separate Electron positive reference covers the common listener/clone/
order/reload/close contract; strict argument errors, quotas, diagnostics and timeout
retirement remain Firefly differences. The incoming-event gate remains green.

```sh
cmake --build build/renderer --target check-firefly-ipc-outbound --parallel 3
```

Local macOS Release passes. Linux CI requires this gate; fresh hosted execution and
native Windows desktop remain open. See [ADR 0137](../architecture/0137-main-to-preload-events.md).
