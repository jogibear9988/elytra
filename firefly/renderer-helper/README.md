# Owned renderer helper entry

Entry.cpp reuses the pinned WebContent bootstrap and sandbox. Its VM-initialization
seam installs exactly one renderer service. UpstreamSessionAdapter.cpp composes the
session-state hook without editing Ladybird sources.

PreloadSession.cpp is the production service: it validates bounded application
preload snapshots, selects top-level documents, creates isolated worlds and revokes
them when documents stop being active. DiagnosticSession.cpp is linked only into
the separate test helper and exposes internal probes for native isolation tests.
RendererSession.h is the common service contract; the two implementations remain
separate executable targets in Targets.cmake.

Do not put test capabilities into the production service. Native handles/realm
roots require explicit teardown; callbacks must not revive a detached world.
Public API status is recorded in the [preload reference](../../docs/api/context-bridge.md).

The [native resource session](resources/README.md) now accepts an explicitly granted
navigation endpoint from the trusted control connection in both owned helpers.
It activates no provider without valid native page/navigable/current-ID scope and
supplies no post-commit document capability. The real production-helper DOM load
is checked by check-firefly-resource-renderer; public protocol/main routing remains
open. See [ADR 0122](../../docs/architecture/0122-live-helper-resource-navigation.md).
