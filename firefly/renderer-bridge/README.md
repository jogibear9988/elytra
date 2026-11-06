# Renderer isolation and publication

This is the renderer-side boundary between a document and its isolated preload
world. It does not install main-process privileges or a general IPC transport.
The native service/helper selects the documented public subset.

| Directory | Responsibility |
| --- | --- |
| worlds | Own isolated realms and reject jobs after document/world revocation |
| serialization | Capture/materialize bounded data graphs and validate native wire bytes |
| context | Publish frozen APIs and guarded callbacks/Promise results across worlds |
| loading | Product-only file script module policy with pinned fetch composition |
| callbacks | Checked WebIDL dispatch and callback-owner records |
| callbacks/events | Listener/handler lifetimes and copied event values |
| callbacks/timers | Native timer cancellation tied to a source world |
| callbacks/observers | Observer ownership, delivery and disconnect behavior |
| callbacks/traversal | NodeFilter/iterator/walker callback-world validation |

Headers are included relative to this root, such as worlds/IsolatedWorld.h.
CallbackTargets.cmake selects narrow owned adapters around pinned upstream units.
Keep Ladybird unmodified; review source/ABI guards when updating it. Never move a
JS value between worlds by passing its raw pointer: capture data or use the guarded
publication engine, and revoke endpoints before releasing document owners.

The native isolation/helper gates test actual LibWeb documents, callback dispatch,
GC roots, source removal and malicious values. See the [security guide](../../docs/guides/security.md)
and [world-lifetime decisions](../../docs/architecture/0031-owned-world-promise-job-guard.md).
