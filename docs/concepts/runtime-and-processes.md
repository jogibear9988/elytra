# Runtime and process model

Mite and Firefly share runtime code but serve different applications. Mite is usable
without a graphical environment; Firefly adds native windows and renderer services.
The tested host is macOS ARM64. This page describes the current architecture and its
limits, not complete Node/Electron compatibility.

## Mite: one owning JavaScript thread

The standalone CLI embeds LibJS and its existing Rust runtime through the upstream
C++ API. It does not embed Node/V8 and does not link Firefly, Qt or LibWeb. Native
services use libuv, existing codec/crypto libraries and small owned host adapters.
Application JS enters the VM on its owner thread. Native I/O workers manipulate
native storage and deliver completion back to that thread.

Use Mite for scripts, CLI tools and servers within the [supported API subset](../api/mite.md).
There is no browser DOM in a Mite script. A package requiring document/window,
Chromium or V8 internals needs a different compatibility decision.

## Firefly main process

Firefly embeds the same Mite runtime graph in its native application host. Its main
entry can load supported Node-style modules and import app/BrowserWindow from firefly.
The native event loop pumps Mite jobs without giving renderer documents those host
capabilities. Most Electron desktop services remain absent.

## Renderer processes

Ladybird's LibWeb/LibJS render HTML, CSS and browser JavaScript in separate helper
processes. WebContent, RequestServer and Compositor are actual native processes in
the tested renderer slice. Browser platform coverage differs from Chromium.

```text
CLI app → Mite → LibJS + native services

Desktop main → Mite + Firefly native APIs
                         ↓ process boundary
                  Ladybird renderer/services
                  LibWeb + renderer LibJS
```

Renderer code has no Mite process, require, Buffer or node:fs access. Browser APIs
belong to the renderer; filesystem/OS services belong to the main process. See
[security boundaries](../guides/security.md) before designing application communication.

## Public bridge status

[CommonJS preload/contextBridge](../api/context-bridge.md) provides bounded isolated
renderer API publication. [IPC](../api/ipc.md) supports bounded invoke/handle
request/reply. A preload has no filesystem/network privileges of its own; only
explicit main handlers can perform privileged work after validating the sender. Do not treat a shared DOM
or the Designer sample's test-only report route as a privileged IPC contract.

Electron's [process-model guide](https://www.electronjs.org/docs/latest/tutorial/process-model)
is a useful conceptual baseline for main/renderer responsibilities. Its Node/Chromium
implementation and additional process APIs are not Firefly's engine architecture.

## Ownership and shutdown

Windows and their webContents have lifetimes governed by app events and native
teardown. Timers, sockets and queued native jobs can keep Mite alive. Fatal callbacks
and forced app exit have different shutdown behavior from a graceful close. Continue
with [async and lifetime](async-and-lifetime.md) and the [Firefly reference](../api/firefly.md).
