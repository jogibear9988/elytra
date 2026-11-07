# 0105: Recognize Mite through a pinned Code-OSS source overlay

## Context and decision

Code-OSS's common/platform.ts and common/process.ts recognize a native unsandboxed
runtime through process.versions.node. Mite deliberately exposes its actual
process.versions.mite and LibJS metadata. The unmodified modules misclassify Mite:
platform emits "Unable to resolve platform.", and the process facade substitutes
web defaults such as cwd=/, missing architecture and an empty environment.

Keep runtime identity truthful. Use the existing Python build-tool layer to
prepare a separate, explicit Code-OSS source overlay. Add recognition of
process.versions.mite alongside the existing Node check in exactly two places,
plus the matching optional TypeScript interface field. This changes no native
OS service, browser trust boundary or Ladybird source. It does not claim Electron
identity or enable renderer privileges.

Check the exact pinned source hashes and unique replacement seams before
creating output. Keep every other source byte, record input/output SHA-256 and
revision in ELYTRA-ADAPTATIONS.json, and preserve the MIT license. The upstream
license has an explicit CRLF checkout rule: accept only its two exact pinned
LF/CRLF forms and preserve the supplied bytes. Source files must retain LF.
An existing output or overlap with the source tree is refused.

This is a partial build overlay, not a compiled or runnable Code-OSS application.
It guards only the files it changes; it does not certify an entire source checkout.
Cache/V8 adaptation, desktop identity/API and actual main/workbench startup remain
separate work. See the [adaptation guide](../guides/code-oss-adaptation.md).

## Evidence

Run actual pinned platform.ts, process.ts and nls.ts after build-only compilation
with existing pinned esbuild 0.25.12. This handles upstream const enums and type-only
imports; no algorithm or dependency is replaced by the acceptance harness.
Compare unmodified Node, adapted Node and adapted Mite. Require native OS/arch,
environment/cwd, locale configuration and actual timer behavior, while Mite keeps
versions.node/electron absent. The unadapted Mite control must reproduce the wrong
classification and diagnostic. Drift/duplicate/missing seams and license changes
must reject before output mutation; verify unchanged source files and preserved
license bytes. Local macOS passes all 71 suites; new native execution is pending.
