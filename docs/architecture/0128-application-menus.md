# ADR 0128: Main-owned application menus with native action generations

Status: accepted for a partial implementation. Supersedes the unsupported-menu
status after [ADR 0127](0127-local-crash-reporting.md), not the full Electron surface.

## Context

The real prepared Code-OSS main entry stops at the missing Menu named export.
Its early Menu.setApplicationMenu(null) should suppress a real menu provider, not
an inert compatibility object. Later interactive menus require actual native
selection, checked state and main-thread callbacks without privileged renderer APIs.
Ladybird must remain unmodified and Mite independent of graphical libraries.

## Decision

Keep the public graph/validation in maintained ESM under firefly/core/menu and the
Qt implementation in firefly/native/menu. Export Menu/MenuItem from the actual
main host and explicit Electron aliases. Reuse Qt QMenuBar/QMenu/QAction: a native
macOS application bar and per-window bars on Linux. No new menu dependency or
Ladybird source patch is required.

Publish bounded schema-one JSON with monotonic int32 generations and command IDs.
Validate it independently before replacing any native tree. Qt retains no JS
objects. Queue selections through the existing main bridge; route focus only to
an owned live BrowserWindow. JS drops events from retired generations and applies
checkbox state before calling application code. Propagate native checkbox state
to sibling bars and future windows. Dispose bars/actions before bridge teardown.

Allow property changes, but reject installed structural edits, repeated ownership,
cycles/shared submenus within a tree and unknown option/type/role requests. Start
with normal/checkbox/separator/submenu and quit/close roles. Do not claim popup,
accelerator, radio or general role compatibility. Require proper new native
implementations and acceptance before expanding the API.

## Evidence and consequences

Local macOS Release policy tests pass in Node/Mite. A real Qt/LibJS probe accepts
native QAction dispatch, checked state and publication, disabled/hidden suppression,
roles, new-window attachment and stale-generation teardown. The public main-host
gate accepts ESM/CommonJS constructors, null before readiness, real installation,
property publication and removal without constructing a BrowserWindow.
Physical OS menu/focus/keyboard behavior and fresh Linux hosted execution remain
required. Native Windows desktop is unavailable. The complete menus example stays
blocked on context-menu/popup support. This slice is import/component progress;
The real Code-OSS main entry now stops at missing contentTracing (exit 1).
Code-OSS's remaining main services and workbench are not accepted.

Native menu trees are rebuilt on property changes, making retirement deterministic
but discarding in-flight old selections. Bounds, immutable installed structure and
current validation scope are part of the [public contract](../api/menu.md).
