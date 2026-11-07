# ADR 0127: Reuse upstream local crash diagnostics through the main API

Status: implemented local-only subset with native macOS Release acceptance. Linux
needs fresh native execution; Windows capture, uploads and workbench startup remain open.

## Decision

Export a main-only crashReporter with real local start, disabled-upload state and
initial annotation snapshots. Add app.getPath/setPath for crashDumps only. Require
explicit uploadToServer: false; refuse suppression of the operating-system handler.
No fake Crashpad/V8 reports, uploaded IDs, empty report-list facade or engine identity.

Reuse LibCore's preinstalled signal-safe record capture and LibWebView's private
report storage/recovery. C++ is appropriate at this existing descriptor/signal/
process boundary; add no extra FFI or bundled crash library. Main starts its own
upstream browser handler with an owned directory. Upstream helper report descriptors
remain parent-owned, and helper report saves use the configured directory.

Compose two exact pinned upstream sources: optional default-directory selection,
application metadata in captured build descriptions, and a fallback global-metadata
append for helper reports created before configuration. Fingerprint both whole
sources, require unique seams, restore every original byte and reject drift. Factor
the translation-unit ownership check into SourceAdapter.cmake for origin/crash
compositions. Preserve all upstream capture/storage/sanitization/license bytes;
Ladybird remains unmodified.

Publish one bounded immutable native configuration for process lifetime. It contains
no VM objects, callbacks or page-derived state. Main-only extra remains distinct
from globalExtra; global keys win in the saved main annotation set. Store escaped
ASCII JSON in raw descriptions because upstream rejects nonprintable/non-ASCII
build records. Preserve Unicode semantics while keeping next-launch recovery tied
to the crashed launch, not the recovering launch's current metadata. Report parsing
and metadata formatting happen outside fatal handlers; the original handler still
writes fixed bounded records and preserves OS signal termination.

Do not destroy signal descriptors or pretend to reset native state after a partial
initialization failure. Successful start freezes directory/metadata; dynamic extras
and report-list APIs remain absent. Local report format, stricter bounds, default
Firefly name/shared directory, early-helper behavior and preserved OS handler are
explicit differences from broader Electron Crashpad behavior.

## Evidence and limits

The actual Firefly public module/alias test configures a Unicode directory, starts
reporting, then waits for its test parent to send SIGABRT to that owned main process.
The next clean launch recovers its private raw record. Verify real native signal/
stack fields, original Unicode product/annotations, global precedence, private file/
directory modes and that recovery does not substitute the new product identity.
Check no environment sentinel or repository/temp directory paths are in diagnostics.
Clean exits remove their pending raw records.

A separate public run creates a real hidden-window renderer before reporting starts.
Verify its actual PID ancestry, externally crash that owned helper, await the real
render-process-gone event and inspect the saved WebContent report. Only global
annotations appear; main-only extra is absent. This exercises actual helper save
routing and the pre-configuration metadata fallback, not a fabricated report file.
Pure Node/Mite policy tests cover validation, snapshots, refusals and native failures.

Mac Release requires the gate; experimental Linux executes it in the existing Xvfb
runtime stage. Shared Windows source compilation still needs CI and does not imply
native capture. The actual pinned Code-OSS import now resolves crashReporter and
fails at missing Menu, retaining exit 1. Its crash options include unsupported OS
handler suppression. No mainImpl/workbench execution is accepted by an import advance.
