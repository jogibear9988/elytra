# ADR 0137: Native current-document main-to-preload events

Status: accepted for the locally tested macOS isolated-preload subset.

## Context

Code-OSS needs preload on/once/removeListener and main event delivery as well as
send/invoke. Main must not require an application IPC request before it can reach
a loaded preload. Queued messages must not enter a replacement document, retain
dead realms or grow indefinitely when the renderer stops acknowledging them.

## Decision

Announce an owned document/realm endpoint natively when its preload exports exist.
Bind through the existing reserved routed seam, using canonical native top-level
page/document ownership and configured preload checks. Control Bind/ACK messages
cannot dispatch app events, consume invoke IDs or rebind a document from an ACK.
Rebuild the owned helper/host together; no Ladybird edits or new upstream seams.

Add webContents.send with synchronous plain-data capture/validation. Record the
native target document/realm at call time. Use a separate monotonic outgoing ID
space and 16-document/64-host ACK quota. Receiver checks identities/replays and
bounds deferred messages to 16. Enter a fresh isolated-world frame rather than
recursing into an executing world. Release credit after synchronous dispatch or
ignored/error delivery; ACKs carry no application result.

Own listener roots in the document endpoint. Accept only source-realm callbacks,
limit registration count to 64, and expose on/once/removeListener/removeAllListeners
with addListener/off aliases. Snapshot before dispatch, remove once before call,
retain listener receiver/data identity and stop when the document/world revokes.
Event sender is the private ipcRenderer object; ports is empty. Block native method
publication through contextBridge. Teardown clears exported object/listener roots.

A 30-second outgoing ACK timeout retires the direction for that document. Do not
replenish credit repeatedly to a stalled peer: that would move unbounded data into
the native transport queue. Release reservations but reject further sends until a
fresh document is loaded. Navigation/window shutdown also reclaim state. This
is credit retirement, not cancellation of effects already queued at the peer.

## Acceptance and remaining work

One hidden-window batch verifies first main send without an app request, cloned
cycles/aliases/UTF-16, listener receiver/event/aliases/order/once, removal/addition
snapshots, root/queue limits, credit recovery, document reload and listener-driven
close. The owned renderer PID is checked against its real OS parent before SIGSTOP;
a real timeout refuses further sends, SIGCONT/reload restores a new endpoint, and
the fixture always reaps its process group. A separate real Electron reference
covers the common positive contract, excluding Firefly's documented limits/errors.
The previous incoming-event public gate still passes. No broad GUI suite is used.

Local macOS Release is accepted. Required Linux CI is wired but fresh execution
and native Windows desktop remain open. This is a preload listener subset, not full
EventEmitter/DOM exception behavior or general contextBridge callback publication.
Binary values, MessagePorts, sync/subframe IPC and Code-OSS workbench startup remain
unfinished. See the detailed [IPC contract](../api/ipc.md).
