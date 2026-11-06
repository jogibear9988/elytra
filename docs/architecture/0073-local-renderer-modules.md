# 0073: Local renderer ES modules without an upstream fork

## Problem and evidence

At the current Ladybird pin, classic file scripts and inline modules execute, but
external file modules and dynamic file imports fail. Fetch treats file origins as
opaque for its same-origin check. It has a specific file-font exception, then
rejects remaining CORS-mode requests whose URL is not HTTP(S). File module requests
therefore never reach its existing file subresource loader. A minimal actual
Firefly/Electron probe isolated the difference before changing the integration.

## Decision

Compose one extra condition in the owned Fetching translation unit. A separate
native FileModules policy is disabled by default and enabled only during the
production renderer helper's lifetime. It accepts CORS-mode Script requests to a
file URL when both the request origin and a Window client's native snapshot come
from a file origin, and the client's creation URL is a file URL. Worker/opaque/
network clients, absent snapshots, other destinations and modes are refused by
this additional condition.

Accepted module requests continue through upstream scheme fetch, file loading,
MIME checks, module parsing/evaluation and CSP. Fetch/file-read refusals are not
removed. No Node loader, JS privilege, webSecurity switch, module inlining or
loopback server replaces the renderer's browser module loader.

The existing native file loader and sandbox still determine accessible paths.
This adapter is not an application-root path sandbox: local script subresources
already use the file loader, and a separate file-access policy remains future
work. Main IPC handlers still require sender authorization.

## Upstream updates

Keep Ladybird untouched. The generator checks the exact pinned Fetching source
hash, finds exactly one condition, and verifies that removing the owned include
and condition reproduces every original byte. Missing/ambiguous seams fail.
CMake replaces exactly one source and preserves all other upstream units. Review
the hook and predicate with each upstream update; remove the condition if upstream
supplies appropriate behavior. Ordinary browser/diagnostic helpers retain the
disabled policy.

## Validation

Actual Electron/Firefly file-page fixtures exercise external static and dynamic
imports, live bindings, module identity/cache, space/Unicode names, reload, strict
MIME refusal, CSP and absent Node capabilities. Firefly-specific pages retain
fetch(file:) refusal and HTTP-to-file module denial. Native policy tests additionally
cover disabled helpers, absent snapshots, network/opaque origins, worker clients,
non-script destinations and modes. The IPC suite now loads external files directly
rather than staging inline modules; its sender/limits/revocation/crash/deadline
assertions remain required. All renderer/isolation/helper and Designer regression
gates remain part of acceptance. No new Windows/Linux desktop claim follows.
