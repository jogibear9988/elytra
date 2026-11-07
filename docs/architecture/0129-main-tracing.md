# ADR 0129: Bounded native main tracing before cross-process profiling

Status: accepted for a partial implementation.

## Context

After native application menus, the prepared actual Code-OSS bootstrap fails on
contentTracing's named export. Its optional tracing mode requests Chromium/V8
sampling or memory categories; no equivalent upstream all-process tracing facility
exists in the pinned Ladybird integration. An inert export would hide missing
behavior and would not help diagnose our real startup work.

## Decision

Add a real main-owned native Recorder and promise-based contentTracing singleton.
Instrument Firefly's actual main-loop pumps, delivered bridge callbacks and native
window creation/destruction requests. Keep categories truthful and reject unsupported
sampling, memory config and Chromium categories. Preserve the path toward later
renderer/helper collection rather than pretending main scopes are all-process data.

Use monotonic C++ clock durations and trace-event JSON with actual PID/logical main
thread metadata. Retain at most 8,192 matching completed scopes in first-events or
ring mode; report discarded/overwritten events explicitly. Independently validate
native config and keep no JS roots, payloads, paths or environment data in scopes.
A generation captured on entry prevents an old scope from landing in a recording
started inside that scope. Disabled tracing does not allocate event strings or
record synthetic placeholder events.

The main bridge owns the recorder. Teardown resets it before VM destruction.
Install controls only on the trusted main host. Existing Mite async filesystem
writes bounded finalized snapshots; an I/O failure retains the same snapshot for
retry and blocks a new start. Optional output uses a native private temporary
directory and exclusive new file. Explicit paths follow normal filesystem overwrite
semantics. No file or new renderer capability is created merely by starting capture.

## Consequences and acceptance

This provides actual Firefly startup/bridge diagnostics and resolves a real import
only after useful behavior exists. It is not general Electron tracing compatibility.
The existing factory receives the real main fs singleton explicitly instead of
creating another filesystem implementation or embedding Node. Standalone Mite gains
no graphical dependency. All changes are owned Firefly files; Ladybird is untouched.

Native tests measure actual time and exercise exact filters, both full-buffer modes,
loss accounting and generation retirement. The public main gate inspects real output,
actual PID/timestamps/durations, aliases, output retry and private modes without a
BrowserWindow. The Node/Mite policy separately tests state/config errors. Fresh native
platform evidence and the next actual Code-OSS blocker are recorded in PROGRESS.

Remaining work includes real WebContent/helper events, clock correlation, sampling,
heap diagnostics and current Electron Perfetto protobuf output. The public contract
spells out all implemented [contentTracing](../api/content-tracing.md) limits.
