# ADR 0176: Native policy observation without a Node addon

## Decision

Expose a small Mite extension, mite:policy-watcher, backed by native platform
providers. Keep it independent of Firefly/LibWeb and do not implement V8 stack
helpers merely to enter an unsupported Node/N-API loader. The maintained ESM facade
preserves the pinned policy-watcher createWatcher/dispose contract and scalar versus
forced-union behavior. Unsupported platforms fail explicitly.

C++ directly connects CF/FSEvents to the existing LibJS/libuv owner boundary and
keeps one native ownership model; an additional language/FFI boundary is unnecessary
for this subsystem.

macOS uses CFPreferences and FSEvents. A serial GCD queue reads exact native types
and publishes a bounded latest full snapshot under a mutex. Workers hold no JS
objects, roots or application callbacks. Native failures latch until disposal;
later notifications cannot overwrite a failed read with successful empty data.
The owner libuv timer consumes real completed snapshots and enters JS only through
EventLoop.dispatch. No completed snapshot and an observed empty snapshot are
distinct states. The facade derives changes/removals without inventing initial data.

Disposal revokes JS roots before stopping/draining the native observer. The native
timer storage survives until uv_close drains. Provider stop handles owner/worker
reentry and repeated calls without a self-deadlock. FSEvents Stop is called only
after successful Start, followed by Invalidate and Release on the serial callback
queue, following [Apple's lifecycle contract](https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/FSEvents_ProgGuide/UsingtheFSEventsFramework/UsingtheFSEventsFramework.html).
Existing preference data and OS settings are never modified.

## Explicit application adapter

Code-OSS requires @vscode/policy-watcher 1.7.0, whose ordinary entry invokes a
Node addon through bindings. The optional build flag --mite-policy-watcher verifies
the exact original package/index, then adapts only a fresh compiled dependency copy.
Mite loads its native extension; Node/Electron keep the original branch. Preserve
licenses and record package/adapter/source/output hashes in ELYTRA-COMPILE.json.
Do not alter the pinned application source, prepared dependency tree, global Node
resolution or arbitrary packages. Updating the package requires another explicit
audit. General native addons and Error.captureStackTrace remain unsupported.

## Evidence and outstanding scope

Actual Node/Mite facade and build-adapter tests, exact native Host subprocesses,
controlled CF/FSEvents provider peers, and real passive macOS initialization pass.
The unchanged pinned NativePolicyService and NativeManagedSettingsService initialize,
replace definitions and dispose through the actual extension, with 46 compiled
dependencies and the conditional package adapter verified. No injected initial
snapshots, preference writes or visible GUI tests are used.

The actual adapted Code-OSS main now advances to ipc.net.serve during claimInstance
and rejects local Unix sockets/named pipes in node:net. Implement genuine local IPC
transport next, including ownership and exact socket errors. A workbench and
workspace open/edit/save remain unaccepted. Real MDM event delivery, Windows registry
observation and any future Linux backend still need implementation/acceptance.

See the [API contract](../api/policy-watcher.md) for exact limits and differences.
