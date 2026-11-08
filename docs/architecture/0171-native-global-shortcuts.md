# ADR 0171: Owned native global shortcuts

## Decision

Keep accelerator parsing and callback ownership in a maintained ESM factory. Install
private LibJS native functions in the main desktop Host, backed on macOS by genuine
exclusive Carbon hotkeys. Map characters using the current TIS Unicode layout rather
than assuming US hardware positions. Keep Ladybird unchanged and Mite independent.

Use monotonically increasing logical tokens and distinct process-wide Carbon IDs.
Native callbacks never retain VM objects. Queue bounded token notifications to the
Qt owner, validate active ownership again on delivery, and revoke before context/VM
shutdown or receiver destruction. Keep cleanup failures inactive and bounded instead
of dropping ownership or reusing an ID that could receive a stale event.

## Alternatives and consequences

Qt shortcuts only observe application-local keys and cannot implement globalShortcut.
Accessibility event taps would introduce broader input privileges; they are not
needed for ordinary hotkeys. Media, live layout rebinding, Windows RegisterHotKey and
Linux X11/Wayland providers remain separate deliverables with explicit failure now.

Tests use exact production Host/provider objects with controlled OS peers to exercise
failures and stale events without disrupting the user's desktop. A separate optional
real registration probe verifies OS admission/release without keyboard injection.
The pinned actual Code-OSS consumer receives a controlled registry through its
existing constructor seam, with its real Electron import graph supplied by Firefly.
Component acceptance never implies workbench/open-edit-save acceptance.

See the [public contract](../api/global-shortcut.md) for limits, errors and differences.
