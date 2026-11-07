# Native main-process bridge

[Bridge.h](../Bridge.h) and [Bridge.cpp](../Bridge.cpp) are the public native
facade. The implementation is private to this directory and still has one owner,
one window registry, one Qt callback context and one set of rooted JS exports.
Splitting translation units does not split ownership or create a second runtime.

| Source | Responsibility |
| --- | --- |
| [Impl.h](Impl.h) | Private owner/window state and internal method contracts |
| [Lifecycle.cpp](Lifecycle.cpp) | Hook registration, queued VM delivery and teardown |
| [Windows.cpp](Windows.cpp) | Native windows/views, chrome observations and navigation callbacks |
| [Preload.cpp](Preload.cpp) | Path/byte/budget validation and native preload session configuration |
| [Ipc.cpp](Ipc.cpp) | Canonical sender checks, document/realm ownership, budgets and native reply transport |
| [Host.cpp](Host.cpp) | Trusted main-process bindings and existing OS-service installation |
| [Value.h](Value.h) | Shared UTF-8-to-VM text conversion |

All operations run on the existing Mite/Qt owner thread. Application callbacks
are queued while the VM is busy. stop() disables native hooks, destroys registered
windows, cancels the owned QObject context, then releases JS roots. Navigation,
crash and destruction invalidate pending native IPC slots and sender identities.
Keep these orderings explicit when changing a subsystem.

The host object remains private to the main process. Renderer/preload access
continues through the separately validated IPC/contextBridge adapters. Native
screen/theme installation stays behind the platform service boundary; standalone
Mite has no dependency on this directory or any Qt/LibWeb source.
