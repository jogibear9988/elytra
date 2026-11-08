# ADR 0148: One-shot native chooser request ownership

Status: accepted as a locally tested native engine component. Native IPC request/
response transport and public webUtils.getPathForFile remain unfinished.

## Context

A path handoff cannot be authorized only by a filename, byte digest or list of active
documents. A delayed response could otherwise complete a replacement picker or a
sibling document. Bind the result to the original engine pending input before adding
the trusted parent/helper message path. Preserve ordinary SDK/browser file selection
when no owned sender is installed.

## Decision

Add native request bookkeeping next to the File provenance registry. A checked owned
Page composition records accepted requests after the engine installs its pending
input and retires them when the engine closes that request. A native sender receives
only the Page and a monotonic serial/document ticket. Sender replacement clears
pending records but never resets the serial. Native callbacks enqueue/capture only;
this interface does not promise reentrant Page mutation from the sender callback.

Pending entries hold weak Page/input/Document references and the original multiple-
selection mode. Native checks remove collected, inactive, adopted or rehosted targets.
Bound the pending table to 64 pages and refuse serial overflow. The parent controller
must additionally retire dialogs on navigation/crash/close; this component does not
observe every inactive/reactivated transition between native checks.

Delivery first matches exact Page, serial and document without consuming a current
request for a stale/foreign ticket. Consume a matching ticket before callbacks. Root
its Page/input/document locally, require the original live document, and validate
metadata against that one document. Reuse the native origin schema and shared chooser
limits: at most 16 byte-backed files, 4 MiB/file and 16 MiB total. Single-file requests
reject multiple files even if the input attribute changed after opening. FD-backed
in-process payloads refuse; a future wire codec must also avoid unbounded descriptor
decode before these checks.

For a matching ticket, malformed payloads cancel the original selection without data.
A synchronous delivery guard requires the exact original input cell and selected span
before Page can call did_select_files. A valid batch runs inside the existing native
origin scope, creating actual Files with known paths. Unknown/obsolete responses cannot
retire a newer request. No sender leaves the upstream byte-selection route unchanged;
that route does not invent path metadata.

The three shared byte/count limits now live in a pure AK header used by the Qt picker
and the native delivery guard. No Qt or Mite dependency enters LibWeb. The generated
Page source preserves all upstream bytes except checked request/close/delivery guards.
The Ladybird checkout and Rust ABI remain unmodified.

## Evidence

The real engine isolation suite drives Page::did_request_file_picker and the actual
pending-target path. Controlled native callbacks capture tickets; controlled metadata
and selected bytes pass through Page::file_picker_closed into real FileList/File
creation, with a positive native origin lookup. It checks null/GC'd/adopted inputs,
replay and old/new inputs, wrong page/document, cancellation, observed inactivity,
service replacement without serial reuse, malformed metadata, single/multiple mode,
per-file/count/aggregate overflow, and unchanged legacy behavior without a sender.

The initial test factory for a foreign principal realm left its execution context
current, causing settings-frame cleanup verification to fail. Restore its documented
extra context before delivery; this was a test setup fix, not an accepted runtime pass.
The complete isolation suite and local macOS Release build then pass. A focused hidden
Qt-view/owned SDK-helper regression retains its actual 4 MiB text/binary/basename/
cancel/navigation behavior. No physical desktop input automation runs. Fresh Linux/
native Windows and 64 simultaneously pending real pages remain unverified.

## Remaining integration

Install the helper request sender and add owned native control messages in both
directions, preserving upstream and prior message IDs. The parent must validate the
actual connection/page/canonical document, retain the original ticket with the dialog,
and return actual accepted paths with the exact bounded byte batch. Keep paths out
of browser filenames and generic application IPC. Validate stale/replaced connection,
subframe and transport ordering cases before claiming chooser-to-helper provenance.
Then add narrowly validated File wrapper passage and the actual public webUtils API.
This component does not advance the last real Code-OSS entry beyond allowServiceWorkers
or establish a workbench. Process events/metrics, MessagePorts, worker/cache/resource
contracts, native platforms and the original full product goal remain outstanding.
