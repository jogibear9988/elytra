# Native release builds

The Native release GitHub Actions workflow responds to version tags (`v*`) and
manual dispatch for an existing version tag. It compiles Mite and Firefly from their
pinned sources, builds the pinned Web Component Designer sample and validates native
behavior before packaging. There is no Node/V8/Electron runtime fallback.

**Current limitation:** native Windows Firefly window/process/sandbox adapters and
packaging are not implemented. The required Windows job will fail explicitly, so
the workflow cannot yet publish a complete macOS + Windows release. There are no
skip/continue-on-error success paths for a required native platform. Workflow syntax
is checked locally, but hosted release builds are not yet validated. This is release
infrastructure in progress, not a claim of available cross-platform binaries.

## Measured macOS development packages

The macOS Release packages workflow builds master in Release mode, runs the native
runtime/renderer/security/sample gates and uploads verified archives plus size
reports. It defaults to production npm dependencies, browser bundling and stripped
nonessential native symbols. The job summary shows uncompressed framework/Designer
bytes and compressed download sizes, rather than assuming that Ladybird is smaller
than Electron. Build prerequisites remain external tools.

Manual dispatch can build an exact existing tag and publish a **macOS-only alpha**
when publish_alpha is selected and the tag matches vX.Y.Z-alpha.N. Publication runs
only after the full native/package job passes; no existing release is overwritten.
This explicitly limited alpha does not satisfy the requested Windows/macOS release.
The all-platform Native release workflow below retains its required Windows gate.
No hosted alpha is available until an actual successful build/publication is recorded.

See [package size analysis](package-size.md) for the measured baseline and what is
already stripped/shared versus remaining optimization candidates.

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

Use fresh build/release-stage, build/designer-app, build/archive-* and
build/release-assets directories; the scripts reject existing output instead of
silently mixing versions. The driver checks the tag and tracked source state,
bootstraps pinned dependencies, configures a Release Mite build, runs CTest, builds
Firefly and its renderer/isolation/helper gates, compiles the demo, tests it, packages
and tests the relocated app, then creates archives/checksum manifests. Renderer build
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
physical renderer resources, all upstream helper executables and internal dynamic
libraries. Our Firefly adapter prefers adjacent helpers/resources in that layout;
the development layout retains its original upstream bundle. There is no source
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
