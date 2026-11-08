# Native stat polling for missing-path recovery

## Decision

Implement fs.watchFile/unwatchFile with C++ libuv uv_fs_poll handles and maintained
ESM registration policy. The actual pinned Code-OSS baseWatcher imports these APIs
for suspended/missing-path recovery. Repeated JavaScript timers around stat would
separate cancellation from native request lifetime and duplicate libuv polling.
Mite remains independent of Node, V8, Firefly and LibWeb; Ladybird stays unmodified.

Share one JS StatWatcher per resolved path and let the first registration select
interval/persistence/BigIntStats. Reuse the existing lossless native stat projection
and JS Stats conversion. Native metadata, including libuv's zero/error and last-real
history, drives callbacks; JavaScript must not invent successful filesystem state.

## Ownership

Each native handle owns a rooted callback and shares its service state. Stop
removes it from active lookup, retires the root and calls uv_fs_poll_stop/uv_close.
The native Watch allocation and service owner survive until the actual close callback,
including outstanding internal stat work. Shutdown stops every admitted watcher
before realm destruction. Native callbacks dispatch only through the owning loop.
Reference state controls loop liveness independently of GC rooting.

Bound active plus closing native handles to 256; never reuse IDs and reject unsafe
ID exhaustion. Report actual libuv admission errors. An admission failure removes
the JS registration, while a stale completion cannot reactivate a stopped watcher.

## Validation and remaining scope

Real Node/Mite tests cover metadata changes, shared and duplicate listeners, removal,
initially missing files, deletion/recreation history, BigIntStats, path/options
validation, shared references and natural exit. Component results and the next
actual desktop blocker are recorded in [progress](../PROGRESS.md).

This is polling, not fs.watch, recursive watching, parcel watcher replacement or
proof of a running Code-OSS watcher service/workbench. Fresh Linux/Windows native
execution remains required; see the [API contract](../api/stat-watcher.md).
