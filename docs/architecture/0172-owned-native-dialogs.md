# ADR 0172: Owned native dialogs for main-process applications

## Decision

Use the existing Electron dialog method shapes with a maintained ESM policy factory,
a strict private LibJS native Host and plain-data native providers. Qt message boxes
preserve original button IDs and checkbox state. macOS file panels use public
NSOpenPanel/NSSavePanel APIs to support Code-OSS mixed file/folder selection and
package traversal. Keep Ladybird unchanged and Mite free of desktop dependencies.

Resolve optional parent IDs through real top-level native window state, revalidate
before delivery, and cancel/reject on parent destruction. Root promises only in the
JavaScript owner. Native worker callbacks queue integer IDs and bounded snapshots;
shutdown revokes callbacks before providers or the VM disappear. IDs are never reused.

Synchronous message boxes are genuinely native modal operations. The bridge defers
VM pumping and JavaScript event dispatch throughout them, including initial-entry
showErrorBox. The dialog Host also defers its own completed requests until the modal
operation returns. Native UI processing must never recursively execute JS callbacks.

## Consequences

HTML input snapshots are not reused: their basename/regular-file/byte-copy restrictions
would break privileged folder and save-destination selection. File dialogs return
native paths only to the trusted main process and never automatically to renderers.
Unsupported security bookmarks, options and platform providers fail explicitly.

Separate controlled failure/lifetime tests from real UI acceptance, which is optional
and batched to avoid repeated desktop disruption. Pure Code-OSS option processing is
not a full dialog-service or workbench pass. See the [contract](../api/dialog.md).
