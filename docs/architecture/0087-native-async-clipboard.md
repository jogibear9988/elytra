# ADR 0087: Native clipboard byte snapshots behind Electron 45 promises

Status: implemented partial macOS slice; private native and shared factory tests
pass locally, actual public desktop fixture pending hosted acceptance.

## Decision

Target the pinned Electron 45.2.3 clipboard/ClipboardItem documentation. Its public
API uses asynchronous readText/writeText/read/write/has and MIME representations;
do not add the older synchronous text/HTML/RTF API as a supposed current baseline.
Keep explicit clear synchronous and selection absent on macOS.

Implement the main-process policy in commented ESM under firefly/core/clipboard.
Capture item records privately, resolve strings/Blobs/promises before publication,
bound all representation bytes and expose immutable read snapshots. Initially
support text/plain, text/html and text/rtf. Reject requested unknown formats and
native read items with unsupported formats. No implicit file-reference or raw
platform clipboard capability is added to renderer IPC.

Use Objective-C++ AppKit providers under firefly/native/clipboard and a private
bounded JSON/base64 host boundary. AppKit supplies actual clipboard state and
publication; no native state retains JS pointers. Work remains on the application
thread; promises provide payload and API compatibility, not a background OS pool.
Normalize AppKit's external UTF-16 text alias and deduplicate automatic UTF-8
aliases; preserve raw RTF/HTML bytes. Detect changed clipboard generations during
multi-format reads. Prepare all data before native ownership transfer, while
reporting OS write failure without promising rollback of clear/writeObjects.

## Validation and limits

A QCoreApplication-only native probe uses pasteboardWithUniqueName and releases
it globally after the test. It never accesses the user's general clipboard or
opens a window. Independent AppKit reads verify actual stored bytes; external
writers, aliases, malformed/unsupported/duplicate/oversized preparation and clear
exercise the OS boundary. Node/Mite tests run the actual full Firefly API factory
against a deterministic provider and check promises, snapshots and failure timing.

The shared public Electron/Firefly fixture changes the general clipboard only
with explicit CI opt-in. The existing desktop test harness runs it on hosted CI
and reports it as not run locally. Native Windows/Linux, images/files/custom
formats, bookmarks/find, full class/coercion behavior, provider blocking/races and
broader cross-app interoperability remain open. Eight-MiB byte and host wire
budgets are not total OS/provider memory quotas. Ladybird and standalone Mite
remain unchanged and independent of this desktop service.
