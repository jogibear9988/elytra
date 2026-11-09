# webFrameMain and webContents.mainFrame

Status: partial, read-only native frame identity on the main process. Local macOS
Release acceptance covers actual main/same/cross-origin frames and admitted preload
IPC; fresh native Linux/Windows execution remains required. This is an active
native document model, not complete Chromium RenderFrameHost/BFCache parity.

```js
import { app, BrowserWindow, webFrameMain } from 'firefly';
await app.whenReady();
const window = new BrowserWindow();
await window.loadFile('index.html');
const frame = window.webContents.mainFrame;
console.log(frame.url, frame.processId, frame.osProcessId);
console.log(webFrameMain.fromId(frame.processId, frame.routingId) === frame); // true
```

## Identity and lookup

webContents.mainFrame returns the actual hosted main-document proxy before any
application preload IPC has been sent. It can return null while no native document
host exists. Unbound child placeholders are not invented as active frames.

webFrameMain.fromId(processId, routingId) returns the same live proxy, or undefined
when no such active frame exists. IDs must be positive integers; they are opaque
identities, not positions in an array. processId is the native engine's internal
managed-child identity. It is **not** the OS process ID or webContents ID.
osProcessId reports the actual renderer PID separately.

| Property or method | Supported behavior |
| --- | --- |
| url | Current canonical document URL, including same-document changes |
| origin | Native serialized origin; opaque origins serialize as null text |
| processId | Actual internal renderer identity |
| osProcessId | Actual renderer OS PID |
| routingId | Document-scoped native routing identity |
| frameTreeNodeId | Native canonical frame-node identity; survives document replacement on that node |
| parent | Canonical parent frame, or null at the root |
| top | Canonical root frame |
| frames | A fresh array of directly hosted child-frame proxies |
| framesInSubtree | A fresh depth-first array including this frame and its hosted descendants |
| isDestroyed() | True when this proxy no longer describes an active owned native document |

Metadata is read-only and revalidated on access. Same-document history/hash updates
retain the proxy and update its URL. A replaced document retires the old document
proxy; reading its properties throws ERR_FIREFLY_OBJECT_DESTROYED. Closing the owner
or shutting down also retires it. Renderer-process swaps use actual native hosts.
Do not infer Chromium frame-host reuse behavior from this document-scoped subset.

## IPC identity

For the existing admitted preload IPC paths, event.senderFrame is the same
canonical proxy as the corresponding current frame. The framework resolves the
native document ID; it does not construct a frame from a packet's URL. Initial IPC
channel binding does not replace an otherwise unchanged native frame identity.
Queued packets from retired documents do not invoke application event listeners.
Invoke requests with unavailable senders reject instead of fabricating metadata.

This does not enable subframe preload IPC, port transfer or another privileged
operation. A frame descriptor itself is not a filesystem or network capability.

## Bounds and unsupported operations

Native snapshots cover active, hosted frames with live ancestor hosts. They are
limited to 256 frames per contents, 1,024 tracked native frame/tree records, 4,096
candidate nodes and 128 ancestor levels. URL/origin fields are limited to 64 KiB
UTF-8 and a snapshot to 2 MiB. Invalid or oversized snapshots fail with
ERR_FIREFLY_FRAME_DATA; identities are never silently truncated or fabricated.
Native document/routing identities are monotonic 31-bit values and never wrap.

send, postMessage, executeJavaScript and collectJavaScriptCallStack remain explicitly
unsupported on these proxies. Frame events, detached/cached frame-host behavior,
per-frame script execution and complete Electron validation semantics remain open.
Workers and unbound native placeholders do not become window-frame proxies.

## Validation

Controlled factory cases pass Node and Mite, including malformed snapshots,
identity collisions, ancestry cycles, immutable metadata and stale ownership.
The real offscreen fixture verifies native identity before application IPC,
same/cross-origin ancestry, internal/OS process distinction, identity lookup,
same-document retention, document replacement, canonical IPC aliases and closure.
The shared hidden Electron 45.2.3 fixture passes; Firefly's document replacement
semantics are asserted separately from Chromium frame-host reuse.

Reference: [Electron webFrameMain API](https://www.electronjs.org/docs/latest/api/web-frame-main).
