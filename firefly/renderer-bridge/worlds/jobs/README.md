# World job and callback policies

[WorldJobGuard](../WorldJobGuard.h) registers isolated worlds and exposes existing
native diagnostics. Its private State owns the original VM hooks and callback gate;
all policies share that same state. VM hooks and queued jobs retain it through VM
teardown, while realm/world/native-object registries remain weak.

| Source | Responsibility |
| --- | --- |
| [State.h](State.h) | Shared weak records, counters and private policy contracts |
| [Registration.cpp](Registration.cpp) | Install/restore the single WebIDL callback gate |
| [Principal.cpp](Principal.cpp) | Save/restore source contexts around principal work and redact source failures |
| [Dispatch.cpp](Dispatch.cpp) | Job/callback ownership, microtask admission and rejection provenance |
| [EventValues.cpp](EventValues.cpp) | Native event tracing, bounded snapshots, cache and revocation |
| [Handlers.cpp](Handlers.cpp) | Private handler-map selection/tracing and rooted cleanup |
| [Listeners.cpp](Listeners.cpp) | Listener/abort registration, cancellation and callback dispatch |
| [Traversal.cpp](Traversal.cpp) | NodeFilter identity, provenance and guarded execution |
| [Resources.cpp](Resources.cpp) | Native timer/observer ownership and detach cleanup |

These are C++ implementation units, not separate services or independent owners.
Preserve the original fallback hooks for unowned/principal work. A source callback
must acquire a live world owner before entering JavaScript; detached documents
cannot resume through jobs, listeners, traversal or native timer/observer delivery.
Do not publish raw source exception/rejection objects to the principal page.

Event values and handler cells rely on explicit native GC visitors. Removing a
record must preserve temporary roots through native cleanup and possible reentry.
Do not replace weak registries with global roots to hide lifetime failures.

The actual windowless LibWeb isolation probe covers source/principal separation,
GC, revoked jobs, callbacks/listeners/handlers, event snapshots, traversal and
resource cancellation. Hosted helper/desktop gates cover the separate renderer
process integration. Neither source organization nor a passing subset establishes
complete security containment or arbitrary platform support.
