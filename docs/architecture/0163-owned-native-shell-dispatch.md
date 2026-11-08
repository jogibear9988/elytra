# Owned native shell dispatch and reversible Trash

Status: implemented for macOS; controlled policy/Host/provider checks and genuine
owned-file Trash/restoration pass locally. Visible launch, activation and Finder
selection are unaccepted. Native Windows/Linux providers are unavailable.

## Decision

Use NSWorkspace URL/open configuration and file-viewer dispatch, and native
NSFileManager Trash on a utility worker. No command-line launcher, command-string
interpolation or permanent-delete fallback is allowed. Maintain a small platform
provider independent of JS and Qt. No Node/V8 dependency or Ladybird modification
is needed. Keep shell out of ordinary renderer and standalone Mite capabilities.

Capture absolute paths on the owner at admission while preserving dot segments
and symlinks. Validate UTF-16/NUL/UTF-8 bounds before conversion. Foundation file
URLs may expose canonically equivalent decomposed Unicode; native fixture checks
URL equivalence without losing spaces, percent signs or fragment-like filename
characters. External URLs use native scheme/handler resolution, with a preflight
to avoid an OS chooser for an unhandled scheme. No general URL allowlist replaces
the caller's responsibility for selecting trusted main-process actions.

## Ownership and completion

Owned monotonic request IDs and pending quotas prevent stale completion reuse.
Each native callback queues a bounded plain result to the Qt owner, then the
existing VM job bridge; it never carries a VM/realm/GC root. Retire delivery before
user callbacks. Provider teardown follows receiver revocation and releases the
posting mutex first, including synchronous teardown callbacks.

Keep a process-global budget of 64 real native callback tickets until actual OS
completion. Cancellation retires JS/host delivery only; it neither refunds a live
OS slot nor rolls back a launch or Trash move. No timer falsely claims native
cancellation. Shutdown rejects JS promises and revokes delivery permanently; late
OS work is still allowed to finish safely without touching a retired VM.

Preserve Electron's result shapes: ordinary openPath errors resolve a message,
whereas openExternal/trashItem errors reject. Validation, unavailable service,
admission, quota, corrupt data and exit errors remain explicit rejections.
Synchronous reveal acknowledges native dispatch, not visible selection. Unsupported
options, beep and Windows shortcuts fail explicitly.

## Acceptance

Actual Node/Mite policy, the exact Host with controlled workers and the exact macOS
provider with intercepted native peers cover admission, errors, early/duplicate/
late completions, quota/cancel/stop/GC and native ticket lifetime. No original
launch/reveal/trash method runs in controlled provider tests.

A separate opt-in genuine Trash test creates one unique temporary file, captures
that operation's resulting URL, restores exactly that file and verifies bytes. It
never enumerates Trash or moves another file. Visible launch and physical UI
acceptance require a later opt-in batch. Non-GUI service gates are wired into the
release build and Linux renderer workflow; fresh hosted execution is not implied.

See the [public API](../api/shell.md),
[Electron shell](https://www.electronjs.org/docs/latest/api/shell) and
[Apple NSWorkspace](https://developer.apple.com/documentation/appkit/nsworkspace).
