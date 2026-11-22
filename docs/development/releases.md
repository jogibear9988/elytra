# Native release builds

The Native release GitHub Actions workflow responds to version tags (`v*`) and
manual dispatch for an existing version tag. It compiles Mite and Firefly from their
pinned sources, builds the pinned Web Component Designer and explicitly partial
Code-OSS samples, and requires native behavior before packaging. Node/pnpm/esbuild
are build tools; there is no Node/V8/Electron runtime fallback.

**Current limitation:** full native Windows desktop/helper confinement and packaging
remain blocked. Win32 providers and guarded AppContainer/broker components exist,
but their new native acceptance is pending; Browser-origin broker retention and
backpressure remain unresolved. A component option cannot open the full runtime
without its separately verified helper-role property. The required Windows job
must fail rather than certify an incomplete desktop, so no complete macOS + Windows
release can yet publish. There are no skip/continue-on-error success paths for a
required native platform.

Historical macOS packages passed at their recorded sources below. The current
four-product pipeline and latest Ladybird candidate are **not** a new native/package
CI pass. Candidate `9803f2d5d888e2afd23d69d1da920f78f4279918` has local standalone
CLI coverage (165 gates); renderer/SDK, relocated Code-OSS and fresh platform/package
acceptance remain separate and pending. No new release is published by this source change.

## Current four-product pipeline

The release driver now stages and archives Mite, Firefly, Web Component Designer
and partial Code-OSS. A successful macOS run must produce exactly four original
ZIPs plus SIZE-macos-arm64.json and SHA256SUMS-macos-arm64.json: six release assets.
The updated promotion verifier requires this four-product inventory. It does not
reinterpret the five assets of the historical three-product alphas below.

Code-OSS is compiled from its exact pinned clean Git source, with selected owned
local-desktop adaptations, generated renderer/NLS assets and only the static
builtin extensions. Its metadata explicitly says that compilation does not
establish desktop acceptance. The builder seals physical metadata, byte hashes,
executable-bit records and internal relative dependency/renderer links; the native
packager verifies that seal before and after copying. See the
[Code-OSS packaging guide](../guides/code-oss-packaging.md) for prerequisites,
commands, source ownership, limits and the exact native gate.

Before packaging, the driver requires the ordinary CJS extension/editor/save,
terminal and parent-liveness workflow through the sample entry with the explicit
offscreen fixture. After packaging and existing native audits, it relocates one
signed Code-OSS.app and launches the **actual native Launcher** on Cocoa. That
launcher supplies its recorded runtime/entry; the fixture supplies only normal
application arguments. Exact Launcher/Firefly executable, birth, UID and owned-tree
authentication, genuine terminal exit and independent saved-file readback remain
required. This new relocated bundle gate is implemented, not yet accepted on a
native CI run. A source-only seal or command-construction test cannot replace it.

## Published macOS Alpha 2

[Alpha 2](https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.2)
is published from successful [package run 42316853522](https://github.com/jogibear9988/elytra/actions/runs/42316853522),
source `19ad11390793278c2bda829180de61ff33980d7f`. The corrected
[promotion run 42345691857](https://github.com/jogibear9988/elytra/actions/runs/42345691857)
verifies the actual source ZIPs and every uploaded asset's size/state/SHA-256,
then publishes the draft. All five original assets retain snapshot version
`0.1.0-dev.118`; no repacking or metadata rewriting occurs.

This includes modern ESM examples, public preload/contextBridge, bounded IPC and
current runtime features at that source. It is **macOS ARM64 only**, ad-hoc signed
and not notarized. Framework: **216.74 MiB**; Designer: **250.85 MiB** uncompressed.
The archives use default Release, without the optional size/shared-provider flags.
Windows/Linux packages and optional-profile acceptance remain separate work.
The subsequent HTTP source refactor and IPC crash-fixture synchronization on master
are not included in these exact release bytes.

## Measured macOS development packages

The macOS Release packages workflow builds master in Release mode, runs the native
runtime/renderer/security/sample gates and uploads verified archives plus size
reports. It defaults to production npm dependencies, browser bundling and stripped
nonessential native symbols. The job summary shows uncompressed framework/Designer
bytes and compressed download sizes (including Code-OSS for the new pipeline),
rather than assuming that Ladybird is smaller than Electron. Build prerequisites remain external tools.

Manual dispatch can build an exact existing tag and publish a **macOS-only alpha**
when publish_alpha is selected and the tag matches vX.Y.Z-alpha.N. Publication runs
only after the full native/package job passes. Assets are uploaded to a draft
first, then publication occurs only when every upload succeeds; an existing
published release is never overwritten.
This explicitly limited alpha does not satisfy the requested Windows/macOS release.
Alpha tags are excluded from the all-platform tag trigger and use the explicitly
scoped manual macOS path. The all-platform Native release workflow below retains
its required Windows gate for other version tags.
The first [macOS ARM64 alpha, v0.1.0-alpha.1](https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.1)
is published from successful [run 42155631670](https://github.com/jogibear9988/elytra/actions/runs/42155631670),
source 745c7af. The initial alpha job was skipped because publish_alpha was false;
the verified artifacts were subsequently promoted without rebuilding or changing
their bytes. Filenames and BUILD.json retain snapshot version 0.1.0-dev.34, with
that provenance stated in the release notes. All five uploaded asset digests match
the downloaded CI artifacts. This snapshot predates newer public preloads, ESM
sample sources and size-profile/shared-OpenSSL changes; it is not current master.
The first size-profile/shared-provider candidate at fd2124e now passes the full
hosted build, with framework 195.94 MiB and Designer 230.05 MiB; see the verified
[artifact/profile details](package-size.md#first-hosted-size-profileshared-provider-acceptance).
It remains an Actions artifact; newer source acceptance is separate.

The macOS release driver now extracts the generated Mite, Designer and Code-OSS
ZIPs into a separate directory for acceptance. It verifies the extracted Mite
revision, verifies the Designer signature, runs the Designer through its
extracted Launcher and runs the editor, extension and terminal checks through
the extracted Code-OSS Launcher. It removes the temporary staging and
successful extraction copies. A failed extraction or sample run fails the
release job before artifact upload.

Manual macOS dispatch also offers optimize_size, default false. It forwards the
explicit C++ size profile to the same release driver; compilation, all native
tests, Designer editing and relocated-package checks remain required. The job
summary reports the recorded C++ size flag and LTO setting with actual package
bytes. This is an experimental build selection, not a claim of a smaller or
validated hosted binary before that particular job passes. Push builds retain
the default optimization profile.

macOS concurrency groups distinguish the source ref/tag, push versus manual
request and size/shared-provider profile. A later ordinary push can therefore
replace another queued ordinary snapshot, while preserving an explicitly queued
experimental profile. Active jobs are never cancelled by this setting. Different
profiles can execute concurrently if runner capacity permits; every profile still
runs all native and package gates. GitHub's default pending-run replacement is
explained in its [concurrency documentation](https://docs.github.com/en/actions/concepts/workflows-and-actions/concurrency).


See [package size analysis](package-size.md) for the measured baseline and what is
already stripped/shared versus remaining optimization candidates.

## Promote an already verified macOS snapshot

The [Promote verified macOS alpha workflow](../../.github/workflows/promote-alpha.yml)
accepts a successful macOS package run ID and an exact vX.Y.Z-alpha.N tag. Its
publish input defaults false: verification emits reviewable provenance/notes and
performs no tag or release mutation. Once a current candidate has passed all
native/package gates, publication can use its original artifacts without another
lengthy native rebuild. This is the reproducible equivalent of alpha.1's promotion.

The verifier requires a completed successful native macOS workflow from this same
repository and a push/manual event. It now checks exactly the four native archives,
SIZE and SHA256SUMS; hashes are streamed. Every archive's root BUILD.json must
match the workflow head commit, filename version, macOS ARM64 platform, known
ad-hoc signing policy and the other archives' complete build metadata. Metadata
is bounded; duplicate JSON keys and ambiguous build records fail. It reads ZIP
metadata without extracting or repacking any payload.

Only snapshots whose archive source matches the workflow head are eligible.
A manually selected different-tag checkout should use the existing tag-build
publication path instead. A successful unrelated workflow, fork, unfinished run,
missing/mixed/tampered artifact or mismatched compressed-size record is rejected.
The source CI remains responsible for actual runtime/renderer/sample/package
acceptance. Verifier policy tests alone never certify native software.

The hosted [promotion dry-run 42300365797](https://github.com/jogibear9988/elytra/actions/runs/42300365797)
passes the actual cross-run download and verifier against alpha.1's original CI
archives (source 745c7af, snapshot 0.1.0-dev.34). Its publication job is explicitly
skipped with publish=false, so it does not change the existing release or tags.
This proves hosted verification/transfer, not remote publication of a newer
candidate. Upload/draft/digest publication remains separately gated.


With publish enabled, a separate write-permission job reverifies the transferred
bytes, creates an annotated tag at the **verified source commit**, and uploads to
a draft. Existing tags cannot move and published releases cannot be overwritten.
Every server-reported asset size/state/SHA-256 digest must match before the draft
becomes public. A failed upload/verification leaves publication incomplete;
rerunning can resume a matching draft. Notes record the actual source/run,
measured package sizes and original snapshot version.

Archive filenames, BUILD.json, application versions, signatures and resource
forks retain their tested snapshot bytes. The alpha release tag does not pretend
to rewrite the embedded version. Windows/Linux desktop delivery remains unfinished;
this path only publishes the explicitly scoped experimental macOS ARM64 alpha.

## Desktop example gate

The release driver additionally builds the [ESM example catalog](../examples/index.md)
for explicit Firefly and pinned Electron runtimes. It compares the catalog's
supported smoke cases plus forced exit, and requires explicit API/platform or
manual-validation rejection for cases that do not claim native acceptance.
Reference-only examples remain manual/unverified. The static asset allowlist is probed by an independent HTTP peer
against Node and Mite. These gates do not request device permissions, perform
purchases, register protocols or modify OS recent-document lists.

## GitHub workflow

After committing and pushing a tested version, create a version tag for that exact
commit and push the tag. The tag workflow builds macOS ARM64 and native Windows x64
in separate jobs. It uses native macOS tools and the Visual Studio clang-cl developer
environment, not WSL. Node/npm/Python/Rust/CMake are build/test prerequisites.

Manual dispatch requires an existing exact tag; the `publish` input defaults false.
When false, validated archives are Actions artifacts only. For a tag push or a manual
publish request, a final job runs only after every required native job succeeds. It
creates a **draft prerelease**, uploads runtime/framework/Designer/partial-Code-OSS
archives and their checksums, and refuses to overwrite an already published release. A failed platform
prevents that final job. Failure logs are retained separately as diagnostic artifacts.
No workflow secret beyond the scoped GitHub token is required for draft upload.

The workflow is on master, the repository's primary implementation branch.
It is available under Actions → Native release once master is the default branch.
Tag events use the workflow checked in at that tag.
No version tag or release was created just to add this infrastructure.

## Local build orchestration

On a native host, with a clean checkout at an existing version tag:

```sh
python3 tooling/build_release.py --tag v0.1.0-dev.1 --jobs 3
```

On macOS ARM64, the release driver enables upstream's fixed Apple M1 CPU baseline
for both Mite and Firefly. It verifies generated C++/Cargo commands, retaining fatal
Rust warnings, and records cpuBaseline for both projects in BUILD.json. Reproduce
the renderer configuration with --baseline-cpu; a standalone Mite CMake build uses
-DENABLE_CI_BASELINE_CPU=ON followed by tooling/check_macos_cpu.py --build BUILD_DIR.
This addresses the ring ARM capability assertion failure in the previous hosted
build; full hosted acceptance remains pending. See
[ADR 0063](../architecture/0063-macos-release-cpu-baseline.md).

Add --lto for an explicit C++ LTO experiment in both projects. The default remains
off while framework-size and native sample validation are in progress. This option
does not skip any release gate or enable the JS JIT/Rust cross-language LTO. The
macOS package BUILD.json records each project's cppLTO setting; package sizes must
be compared with identical dependency/symbol modes. A standalone manual CMake build
uses -DENABLE_LTO_FOR_RELEASE=ON. Unsupported compiler/linker IPO fails explicitly.

The separate --optimize-size experiment applies a checked C++ size flag after
upstream Release optimization options. Rust/dependency profiles are unchanged;
cppSizeOptimization records each project's enabled setting/flag in BUILD.json.
The generated-command guard must confirm the size flag actually takes precedence.
Local framework/sample acceptance passes; hosted profile acceptance is pending,
so this also defaults off. Combining
--lto and --optimize-size requires its own native validation; no size/performance
claim follows merely from selecting either flag.

Use fresh owned build/release-stage, build/designer-app, build/code-oss-app,
build/code-oss-release-overlay, build/archive-* and build/release-assets outputs; the scripts reject existing output instead of
silently mixing versions. The driver checks the tag and tracked source state,
bootstraps pinned dependencies, configures a Release Mite build, runs CTest, builds
Firefly and its renderer/isolation/helper gates, compiles both samples, runs their
required workflows, packages and tests the relocated apps, then creates
archives/checksum manifests. The hello-world
is authored in ESM; packaging builds its sandbox preload with pinned esbuild and
checks the actual ESM main/page against Electron/Firefly plus the native app launcher. Renderer build
is already Release. It stops at the first failed required command. A supported macOS
build may take substantial time/disk space; no complete hosted run of the new
four-product/latest-SDK pipeline is claimed yet. Reuse verified source/dependency
inputs, remove superseded owned outputs only after their processes stop, and
preserve Finder metadata.

## macOS package layout

The current experimental packager consumes existing native outputs:

```sh
python3 tooling/package_macos.py --mite-build build/release-mite \
  --renderer-build build/renderer --designer build/designer-app \
  --code-oss build/code-oss-app \
  --output build/release-stage --version 0.1.0 \
  --dependencies production --symbols stripped
```

With --code-oss, it produces bin/mite, a Firefly.app hello-world application,
a Web Component Designer.app and Code-OSS.app. Direct packager use may omit the
optional Code-OSS input; the current release driver always supplies it. Each app has a small native Finder launcher, the Firefly executable,
physical renderer resources, our production Firefly WebContent helper, the other
upstream helper executables and internal dynamic libraries. Our Firefly adapter
prefers adjacent helpers/resources in that layout; development uses the separate
FireflyRuntimeHelper bundle. Packaging fails if its production WebContent is missing. There is no source
patch to Ladybird and no working-directory/environment override for helper lookup.

Build-machine RPATHs are removed. The packager checks non-system dependencies and
adds only bundle-relative library paths, preserving internal library symlinks.
After relocation it signs libraries/helpers ad hoc while preserving existing helper
entitlements and hardened-runtime flags, then signs/verifies each complete bundle.
It does not weaken the sandbox. Packages are not Developer-ID signed or notarized.
The minimum macOS version is conservatively the build host; support for earlier
versions is unverified despite older upstream link deployment targets.

Mite/Firefly and upstream notices, vcpkg copyrights and Rust package license metadata
are retained. Crates without a standalone notice file include their exact distributed
source archive rather than an invented notice. Demo node_modules retain their
licenses/package metadata and source. Each app carries the common notices internally;
archives also include them separately. This collection is not a full legal/security
audit, certification or FIPS statement.

The packager also accepts --dependencies all and --symbols keep for development.
A bundled input must be rebuilt with the requested dependency mode; an omitted
package cannot be restored from a smaller payload. The module staging mode can be
filtered again at packaging. Production selection follows npm lockfile dev flags,
not guessed directory names; shared runtime and production optional packages stay.
All shipped package notices remain. Symbol stripping retains dynamic exports and
helper entitlements/flags, followed by code-signature validation.

SIZE.json records actual app/component file bytes. The release-assets directory
contains SIZE-macos-arm64.json with all selected bundles and compressed archives.
BUILD.json records version, source/upstream pins, architecture, host/signing details
and inventory. SHA256SUMS.json describes staged regular files; archive checksums are
written after compression. The sample's `--smoke` gate tests document boot/editing,
Monaco and absent Node globals; it is not complete app/worker/collaboration coverage.
An unlocked visible-desktop review and native signing/notarization acceptance remain
open. Windows/Linux packages and installer formats remain implementation work.

For the opt-in macOS ARM64 shared crypto experiment, pass --shared-openssl to the
release driver or select shared_openssl in the macOS workflow. It keeps the same
native/Designer/package gates and adds provider-inventory checks to the selected native apps. BUILD.json records sharedOpenSSL and OPENSSL-*.json records input archive
hashes and physical consumers. Defaults remain off pending hosted acceptance;
this flag does not add a system OpenSSL dependency or change standalone Mite.
See the [measured size report](package-size.md) and
[ADR 0069](../architecture/0069-shared-openssl-provider.md).
