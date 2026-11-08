# ADR 0149: Explicit local desktop Code-OSS experiment

Status: accepted as a locally tested source adaptation; no working workbench.

## Context

The actual pinned desktop entry requests service-worker/code-cache privileges for
vscode-webview and nonstandard remote resource schemes. Ladybird does not execute
the required service workers. Merely accepting the flags or removing them while
leaving consumers enabled would conceal missing contracts. The immediate priority
is a real local editor; full extension/webview/remote workflows remain required.

## Decision

Keep this policy in an owned Code-OSS source overlay, separate from Firefly's public
API. Require --local-desktop together with --libjs-cache and --libjs-engine when
preparing the overlay, plus ELYTRA_CODE_OSS_LOCAL_DESKTOP=1 at runtime on a truthful
LibJS host. Capture the runtime choice once, print its limitations and freeze its
status. Retain the original Node/Electron branches and default LibJS refusal.

Keep only the audited vscode-file origin declaration and its real supported
privileges. Disable the actual engine cache path. Omit native webview protocol and
managed remote initialization; reject both webview factories (including the
Electron override), all three webview main operations, both resolver entry points,
remote resource providers/requests and remote CLI requests. Resolver guards return
rejected Promises; the inactive branch preserves the original Promise identity.
No successful replacement service is fabricated. Inactive consumers remain the
original source algorithms. This does not implement service workers or a webview
fallback and does not change upstream Ladybird.

Pin seven additional original source files with license headers. The preparer
verifies fingerprints before writing output, inserts guards only at unique audited
seams and records original/owned/overlay hashes. Tests reconstruct every original
byte after removing declared changes. Node/esbuild remain build/reference tools;
the application runs under Mite, not Node/V8.

## Validation and remaining work

Local Node/Mite policy tests pass enabled/absent/0/invalid settings, remote CLI and
feature refusals, rejected Promise semantics, untouched original scheme identity,
contract drift and cache behavior. Frozen status/mode and pinned source/duplicate/
missing-seam/output refusal also pass. The native CTest includes this non-GUI gate.

The real compiled source entry with prepared production dependencies completes its
import in enabled mode. Allowing the real app ready callback to proceed reports
missing generated NLS messages and unsupported node:zlib; the bounded diagnostic
always exits 1 and explicitly refuses workbench acceptance. The same tree with mode
0 still fails at allowServiceWorkers. There is no window, open/edit/save acceptance,
renderer/service workflow test or fresh Linux/Windows execution in this slice.
Implement genuine NLS preparation and demanded compression behavior next, continue
the real startup, then accept an actual editor. Keep MessagePorts, preload/File
handoff, extension/PTY hosts and full worker/resource workflows on the full goal.
