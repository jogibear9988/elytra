# Native release builds

The Native release GitHub Actions workflow responds to version tags (`v*`) and
manual dispatch for an existing version tag. It compiles Mite and Firefly from their
pinned sources, builds the pinned Web Component Designer sample and validates native
behavior before packaging. There is no Node/V8/Electron runtime fallback.

**Current limitation:** native Windows Firefly window/process/sandbox adapters and
packaging are not implemented. The required Windows job will fail explicitly, so
the workflow cannot yet publish a complete macOS + Windows release. There are no
skip/continue-on-error success paths for a required native platform. Hosted macOS package builds now pass; the full cross-platform release workflow
remains incomplete. This is not a claim of available Windows/Linux desktop binaries.

## Measured macOS development packages

The macOS Release packages workflow builds master in Release mode, runs the native
runtime/renderer/security/sample gates and uploads verified archives plus size
reports. It defaults to production npm dependencies, browser bundling and stripped
nonessential native symbols. The job summary shows uncompressed framework/Designer
bytes and compressed download sizes, rather than assuming that Ladybird is smaller
than Electron. Build prerequisites remain external tools.

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
repository and a push/manual event. It checks exactly the three native archives,
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

Current master additionally builds the [23 ESM example apps](../examples/index.md)
for explicit Firefly and pinned Electron runtimes. The release driver compares
six supported native slices plus forced exit, and requires explicit API/platform
rejection for the 17 blocked Firefly examples. Reference-only examples remain
manual/unverified. The static asset allowlist is probed by an independent HTTP peer
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
creates a **draft prerelease**, uploads runtime/framework/Designer archives and their
checksums, and refuses to overwrite an already published release. A failed platform
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

Use fresh build/release-stage, build/designer-app, build/archive-* and
build/release-assets directories; the scripts reject existing output instead of
silently mixing versions. The driver checks the tag and tracked source state,
bootstraps pinned dependencies, configures a Release Mite build, runs CTest, builds
Firefly and its renderer/isolation/helper gates, compiles the demo, tests it, packages
and tests the relocated app, then creates archives/checksum manifests. The hello-world
is authored in ESM; packaging builds its sandbox preload with pinned esbuild and
checks the actual ESM main/page against Electron/Firefly plus the native app launcher. Renderer build
is already Release. It stops at the first failed required command. A supported macOS
build may take substantial time/disk space; no complete hosted run is claimed yet.

## macOS package layout

The current experimental packager consumes existing native outputs:

```sh
python3 tooling/package_macos.py --mite-build build/release-mite \
  --renderer-build build/renderer --designer build/designer-app \
  --output build/release-stage --version 0.1.0 \
  --dependencies production --symbols stripped
```

It produces bin/mite, a Firefly.app hello-world application and a Web Component
Designer.app. Each app has a small native Finder launcher, the Firefly executable,
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
contains SIZE-macos-arm64.json with both bundles and compressed archives.
BUILD.json records version, source/upstream pins, architecture, host/signing details
and inventory. SHA256SUMS.json describes staged regular files; archive checksums are
written after compression. The sample's `--smoke` gate tests document boot/editing,
Monaco and absent Node globals; it is not complete app/worker/collaboration coverage.
An unlocked visible-desktop review and native signing/notarization acceptance remain
open. Windows/Linux packages and installer formats remain implementation work.

For the opt-in macOS ARM64 shared crypto experiment, pass --shared-openssl to the
release driver or select shared_openssl in the macOS workflow. It keeps the same
native/Designer/package gates and adds complete provider-inventory checks to both
apps. BUILD.json records sharedOpenSSL and OPENSSL-*.json records input archive
hashes and physical consumers. Defaults remain off pending hosted acceptance;
this flag does not add a system OpenSSL dependency or change standalone Mite.
See the [measured size report](package-size.md) and
[ADR 0069](../architecture/0069-shared-openssl-provider.md).
