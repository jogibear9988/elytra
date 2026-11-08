# Native filesystem events and canonical paths

## Decision

Use C++ libuv uv_fs_event handles for fs.watch and the existing filesystem worker
service for uv_fs_realpath. The actual pinned Code-OSS nodejsWatcherLib needs both
native events and canonical paths. Share maintained ESM argument/error/lifecycle
policy with the existing filesystem facade, without a Node/V8 host or a new thread/
FFI boundary. Keep Ladybird unmodified and Mite independent of Firefly/LibWeb.

Each FSWatcher owns one native handle, unlike path-shared StatWatcher polling.
Native callbacks copy byte filenames or preserve null before dispatch; JavaScript
selects encoding and gives rename precedence when both native flags are present.
Never synthesize filenames or promise one event per operation. Actual Node and
Mite on macOS can report directory **and file** appends as rename; acceptance
asserts eventual target events and separately validates consumer classification.

## Ownership and cancellation

Native active/closing handles have a separate 256-item event-watcher limit. Stop
revokes lookup and roots before uv_fs_event_stop/uv_close; retain storage/service
ownership until actual close. Runtime shutdown closes all active handles before
realm retirement. Native callback-local roots preserve functions and byte arrays
through application close/GC reentry. Unref changes loop liveness, not GC ownership.

Use private AbortSignal subscriptions supplied by the loader. A pre-aborted signal
schedules close after native admission, so genuine startup errors still surface.
Ordinary close emits one nextTick close; an asynchronous native failure closes and
emits error without ordinary close, matching Node's native wrapper.

Recursive events use UV_FS_EVENT_RECURSIVE on macOS/Windows. Linux recursion needs
separate directory-tree ownership and is explicitly unsupported initially. Ignore
filters and promise-iterator watching are also explicit remaining work. Do not
advertise fresh Windows/Linux acceptance from macOS execution.

Canonical path operations reuse copied filesystem requests, rooted completion and
lossless returned byte arrays. Copy uv_fs_realpath ptr before request cleanup;
decode only at the public encoding boundary. Both ordinary and .native forms use
the OS canonicalizer; Node's JavaScript fallback/error details are not replicated.

## Evidence and scope

Actual Node/Mite fixtures cover events, filenames, recursive nested names, abort,
validation, liveness and disposal; separate tests cover real quota recovery,
embedded GC/reentry/reclamation and shutdown. Canonical-path tests cover actual
symlinks/broken targets/loops, bytes/URLs, encodings, ordering and native entry-point
isolation, with filesystem/privilege skips printed explicitly.

The unchanged compiled Code-OSS nodejsWatcherLib passes canonical paths,
create/update/delete, correlation/coalescing and disposal with 39 hashed compiled
dependencies. It is a component, not the complete watcher service/workbench.
See [progress](../PROGRESS.md), [event API](../api/event-watcher.md) and
[canonical paths](../api/realpath.md) for current validation and limitations.
