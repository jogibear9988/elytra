# Asynchronous work and resource lifetime

Mite has a rooted FIFO Promise/microtask queue, a separate priority nextTick queue,
native libuv timers and asynchronous native I/O. This is a tested subset of Node's
event-loop behavior, not a claim that every Node phase/order is identical.

## Work on the JS owner thread

Application callbacks and Promise reactions run on the owning VM thread. Background
I/O workers use native storage and post a completion. They must not access JS heap
objects directly. Firefly pumps this same runtime through its native application loop.

Avoid long synchronous CPU work or synchronous filesystem operations in callbacks
that need responsive networking/windows. Worker threads and child_process are not
yet available as general offloading APIs. Use bounded work and asynchronous services
that actually exist in the current [reference](../api/mite.md).

## What keeps a program alive

Referenced timers, listening/connected sockets and pending native work can keep the
runtime alive. Use close/end for resources with those contracts. Timer handles have
ref/unref/hasRef; an unreferenced timer alone does not guarantee the process stays
alive to invoke it. Timer refresh and immediate timers remain unsupported.

A FileHandle close waits for active references. A Firefly window's destruction
rejects its pending load. Different services have distinct cancellation rules; an
AbortSignal option must not be assumed supported because it exists in Node.

## Errors cross async boundaries

Handle the returned Promise or error callback. Exceptions in an unrelated later
callback are not caught by a try/catch around the operation that scheduled it.
Unhandled rejections and fatal callback errors lead to a nonzero exit after the
runtime's documented checkpoint behavior. Process rejection/exception events and
Node-compatible diagnostic formatting are not implemented.

Normal process.exitCode allows referenced work to drain. Immediate process.exit,
fatal shutdown and Firefly app.exit may terminate with pending work; pending writes
can be abandoned. Tests distinguish this from graceful server/window closure.

## Streams and backpressure

Use pipeline or writable completion callbacks rather than enqueueing unbounded
chunks. The pinned readable-stream adapter provides stream primitives with host
Buffer/events/process integration. Filesystem createReadStream/createWriteStream,
Web Streams and complete AbortSignal integration are not currently available.
The runnable [stream example](../../examples/mite-stream/pipeline.mjs) uses supported
in-memory streams; it does not imply filesystem streaming support.

## Desktop lifecycle

Await app.whenReady before constructing BrowserWindow. Observe close/closed and load
Promise rejections; avoid retaining an object after destruction. Quit cancellation,
forced exit and nested quit requests have explicit measured behavior. See
[app](../api/app.md), [BrowserWindow](../api/browser-window.md) and [webContents](../api/web-contents.md).
