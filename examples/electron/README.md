# Desktop example applications

This directory contains ESM source for every topic in Electron’s examples overview,
plus quick-start and lifecycle. Start with [our catalog](../../docs/examples/index.md)
for build commands, API connections, manual setup and precise coverage.

The default runtime is Firefly. Missing capabilities fail with an explicit error;
there is no automatic Electron fallback. Choose an explicit electron reference build
only when comparing with the pinned development installation. ESM preload sources
are bundled for each runtime’s sandbox; authored sources remain ESM.

Six narrow native/reference smoke paths are tested. The other 17 examples are
reference implementations for missing Firefly capabilities and are not advertised
as working native Firefly features. Tests never register protocols, request devices,
purchase products, modify recent documents or post system notifications.
