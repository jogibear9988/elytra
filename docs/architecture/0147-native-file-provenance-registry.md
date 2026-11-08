# ADR 0147: Weak native input File provenance

Status: accepted as an internal, locally tested macOS LibWeb/LibJS component.
Public webUtils.getPathForFile and main-to-helper path delivery remain unfinished.

## Context

The bounded native picker now delivers real browser Files. SelectedFile and the
Rust File representation still retain only basename/data, not the source path.
A future path API must use native identity, not script-provided names, properties
or bytes. It must distinguish an observed native selection with unavailable origin
from a File never observed at that boundary, without retaining documents forever.

## Decision

Add a small native registry to our LibWeb integration library. Its only File keys
are weak references to exact Rust-backed File cells, with weak originating Document
references and optional trusted path strings. A checked build adapter notifies the
registry immediately after HTMLInputElement creates each native selected File.
It roots the evolving FileList across creation/notification; the registry roots the
current File/document during native work. No JavaScript properties are enumerated,
no hooks run and no file bytes are copied by the registry itself.

A synchronous native Selection scope supplies a versioned JSON schema plus the exact
selected-file span. Require version 1, at most 64 unique canonical nonzero u64:u64
document IDs, at most 16 paths matching that span, absolute NUL-free UTF-8 paths at
most 4,096 bytes each / 64 KiB combined, and matching native basenames. Reject nested
scopes and schema drift. Grant a path only when the actual active document ID, exact
selected span element address and created File's native name all agree. A copied
selection vector with identical data cannot consume the association.

Registration is immutable: an unavailable native File cannot be upgraded later by
an unrelated scope. Lookup checks exact File/document identity. Unobserved Files
return no association only while that classification remains safe; this is not a
general proof that any unobserved File is synthetic. Native selected Files lacking
metadata fail. Inactivation releases path strings but retains a weak negative
File entry, preventing a surviving old File from being reinterpreted as unobserved
on reactivation or reuse. Collected File entries disappear without roots.

Bound entries to 1,024 and path strings to 1 MiB/helper. Quota refusal marks the
originating document unavailable in a bounded weak failure ledger. It also makes
unrecorded identities ambiguous until native lifecycle cleanup: a refused File
might outlive its document without a table slot. Failure-ledger overflow refuses
all lookup rather than losing that negative knowledge. No page-accessible reset
exists. Existing known paths in other nonrefused documents can still be queried.

Keep this separate from general BridgeData and the public preload modules. There
is no new JavaScript path method, DOM clone permission, IPC grant or filesystem
access. The actual parent controller and File-wrapper passage will be separate
validated steps. Default browser/data transport continues refusing DOM/File values.

## Evidence

The real native isolation gate invokes actual HTMLInputElement::did_select_files,
creating actual Rust-backed FileList/File cells through the checked integration
hook. Controlled native test metadata associates a Unicode path. It covers same
name/data noninheritance, wrong span identity, wrong created filename, missing or
wrong-document metadata, no later upgrade, schema/version/type/path/identity errors,
nested scope refusal, document inactivation/reactivation, weak GC removal, exact
1,024-entry and 1 MiB path budgets and fail-closed overflow. These metadata paths
are native test inputs, not evidence of OS chooser-to-helper path delivery.

The whole actual-engine isolation suite and macOS Release runtime build pass.
The hidden native Qt-view/SDK-helper file-selection regression still passes real
4 MiB text/arrayBuffer bytes, basename privacy, cancellation and navigation. No
physical desktop input automation is needed. Fresh Linux/native Windows execution
remains unverified. The generated source retains every upstream byte except the
owned includes/root/notification insertions; fingerprints and missing/duplicate
seam checks fail on drift. Ladybird's checkout and Rust File ABI stay unchanged.

## Remaining integration

Capture and deliver actual accepted chooser paths over a trusted native control
message, bound to the original document(s) and exact selection. Keep metadata and
selected bytes together; do not overload browser filenames or correlate only by
basename/content hashes. Validate stale navigation, cancellation, subframes, wrong
page/connection and replay cases. Then add narrow File wrapper passage for the
required contextBridge call and the real webUtils.getPathForFile API. Ordinary DOM
IPC, native drops/clones, large-file/streaming transfer and measured overhead remain
separate work. No Code-OSS startup/workbench acceptance follows from this component;
its worker/cache/resource and MessagePort prerequisites remain required. Full goal
stays active.
