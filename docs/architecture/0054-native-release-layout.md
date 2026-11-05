# 0054: Owned macOS bundle layout and gated native release workflow

Status: experimental macOS ARM64 bundle/relocation slice tested. Native Windows/
Linux desktop ports, full hosted release runs, notarization and complete cross-platform
publication remain incomplete.

## Decision

Keep packaging, launcher, helper/resource discovery and loader-path fixes in our
adapters/tools. Ladybird's source/gitlink/manifest stay unmodified. A small native
C++ Finder launcher execs the adjacent Firefly runtime and bundled main entry without
assuming a working directory. It introduces no FFI or second JS engine. The existing
Mite/Firefly source graph stays separate; standalone Mite has no renderer dependency.

Firefly prefers its physically adjacent WebContent/resources layout when present;
the source-tree development executable retains the companion Ladybird bundle. The
macOS packager stages every required helper, resources and linked shared libraries,
removes build-machine RPATHs, verifies bundle-relative dependency resolution and
preserves internal library symlinks. Its curated helper inventory is checked against
the pinned upstream CMake helper list so an upgrade cannot silently omit a new helper.
Code signing preserves upstream helper entitlements and flags, checks them against
the originals, and signs/verifies each complete bundle. It never turns off sandboxing.
The minimum OS version is conservatively the build host; older hosts are unverified.

Preserve project/upstream notices, native dependency copyrights and Rust package
metadata. For crates with no standalone notice, retain their distributed source
archive. The demo retains npm package sources/notices. Include notices inside each
app and alongside archives. Record revisions, dirty tracked source, configurations,
architecture, host/signing and hashes. This is not a legal/security audit, FIPS or
Developer-ID/notarization claim.

The release driver requires a clean tracked checkout at an exact existing version
tag. It builds Release Mite, tests it, builds/tests Firefly's native/renderer/isolation/
helper slices, compiles/tests Designer, stages/tests relocated apps and archives them.
The SHA-pinned Actions workflow runs native macOS ARM64 and Windows x64 jobs. Every
required job must pass before a draft prerelease is created/uploaded. Manual dispatch
publishes only when requested; published releases are never overwritten. No tags or
releases are created merely to add infrastructure. Missing Windows support currently
makes the complete release pipeline fail explicitly, not skip into a green release.

## Evidence and limits

Both apps stage with valid ad-hoc bundle signatures. Relocated tests from
build/release candidate/ (including spaces) pass: the framework's existing full
Electron-reference gate in build/firefly-relocated-test.log and the sample's real
boot/document edit/Monaco/absent-Node gate in build/designer-relocated-test.log.
All ten helper entitlements and code-sign flags match their source-build originals.
The ordinary development Firefly gate also passes in build/release-firefly-test.log.

A separate native Release Mite build passes all 35 suites in
build/release-mite-test.log; its dynamic linkage is platform-only. The initial local
bundle candidate used the earlier Debug Mite binary and Release Firefly; the release
driver specifies Release for both. No complete hosted release run or release upload
was performed. Both workflows pass checksum-verified actionlint 1.7.12 locally.

The Designer compositor capture proves a rendered UI; locked-desktop native pixel
presentation still needs an unlocked-session review. Missing/corrupt-resource/provider
paths, full app commands/workers/collaboration/GPU/LLM, older macOS, signing identities,
notarization, installer formats and native Windows/Linux remain open. Public preload/
contextBridge/IPC and the broader specification are still incomplete. No new upstream
candidate or alternative engine was used.
