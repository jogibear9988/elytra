# ADR 0146: Real native file input before path provenance

Status: accepted for bounded local macOS native Qt/helper behavior.

## Evidence

While implementing the pinned Code-OSS preload's webUtils.getPathForFile dependency,
source inspection found that Firefly never attached on_request_file_picker. The
Qt browser frontend attaches this in Tab.cpp, a browser UI component Firefly does
not embed. LibWebView's create_selected_file passes only a basename and descriptor;
SelectedFile decode eagerly reads contents; HTMLInputElement constructs File from
bytes/name/type, and the Rust-backed File has no source-path field. Ordinary bridge
data also correctly refuses File/DOM objects. A file-path method cannot honestly
be implemented by returning a script-provided filename or broadening all DOM capture.

## Decision

First provide the missing actual native file-input operation. Attach one owned
QFileDialog service to each Firefly view, with explicit cancellation on load-start,
main-process load, helper crash and destruction. Invalidate the active QPointer
before dismissal and drop completions before owner destruction. Use Qt's nonmodal
platform chooser; internal test executables can supply a hidden widget factory,
which is not a renderer or application-JavaScript capability.

Open accepted paths nonblocking and fstat their handles before reading. Reject
nonregular inputs without blocking on FIFOs, and read at most 4 MiB plus one byte to
catch growth. Return all-or-nothing byte snapshots with count 16 and aggregate 16 MiB
limits. Send the upstream SelectedFile byte variant, avoiding its unbounded deferred
read of a potentially growing descriptor. Keep names as basenames and leave the
existing engine FileList/File and selected-file IPC unchanged. Advisory accept filters
are bounded; large/directory/streaming file selection remains unsupported.

The original native path stays out of page JavaScript. Do not overload SelectedFile
names with path metadata or use a new placeholder webUtils method. Plan a separately
validated native provenance association, bound to exact File/document identity, plus
explicit File-wrapper passage for that API. Arbitrary DOM values must keep refusing
ordinary bridge capture. The public path API and file-transport performance goal
remain outstanding. Neither Ladybird nor Mite changes in this slice.

## Acceptance and limits

Hidden real Qt dialog and filesystem probes pass binary/Unicode/basename data,
regular/empty/FIFO/directory/type/path/count/byte limits, exact boundaries, cancellation,
stale completion, duplicate request and no completion after destruction. The actual
native view/owned upstream SDK helper gate passes trusted native input activation, accepted-file
SelectedFile IPC, real 4 MiB File.text()/File.arrayBuffer() data including a full
binary scan and embedded NUL, basename/path privacy, cancel and navigation to
an empty new input. It uses the real Qt widget dialog backend with physical display
suppressed; production-helper/interactive platform sheet and JavaScript app/Designer acceptance
are not established. Fresh hosted Linux and native Windows remain unverified.

This performs whole-file bounded copies. It does not accept zero-copy transport,
large files, drop provenance, webUtils.getPathForFile, Code-OSS or full Electron
compatibility. Continue the native provenance/bridge and MessagePort work while
retaining the real-entry worker/cache/resource requirements and full active goal.
