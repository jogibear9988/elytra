# Experimental native Linux app packaging

The native x64 ELF closure component passes its compiled relocation gate at
`209f64e`. This application packager builds on it; whole-application archive
acceptance is a separate, required CI gate. Linux packages are experimental
candidates, not published release assets or a verified portable distribution.
The existing macOS packages and release are described in [releases](releases.md).

**Native acceptance:** source **197cd2a**, [run 42379262065](https://github.com/jogibear9988/elytra/actions/runs/42379262065),
job 128423224720, passes the complete stripped archive and extracted Designer
gates below on Ubuntu 24.04 x64 Xvfb/Openbox. The
[candidate artifact](https://github.com/jogibear9988/elytra/actions/runs/42379262065/artifacts/13577811546)
contains the tested experimental archive/checksum/BUILD.json. Its 122,681,800-byte
Actions ZIP is a compressed upload size, not the installed framework footprint.

## Build inputs and command

Use native Ubuntu 24.04 x64 with the pinned dependencies/compiler, patchelf,
GNU readelf/strip, dpkg-query and the offline Cargo sources used by the renderer build.
First build the actual Release runtime using [Linux renderer bring-up](linux-renderer.md)
and a separate Release Mite CLI using [Linux development](linux-build.md). Configure
the renderer with --baseline-cpu and standalone Mite with
-DENABLE_CI_BASELINE_CPU=ON. Packaging refuses an SDK without this portable
x86-64-v2 policy; generated C++/Cargo commands must pass check_linux_cpu.py.
Mite must be built from the standalone root CMake project. A CLI linked from the
full renderer SDK can acquire LibWeb transitively and is refused by this packager.
No Ladybird patch or alternate JavaScript engine is introduced.

The [DBus provider policy](../architecture/0103-declared-elf-provider-selection.md)
addresses the observed SDK/system libdbus-1.so.3 collision. Declare the frontend's
actual provider and require all consumer imports' symbol versions/types before
staging one copy. Record the selection/proof in BUILD.json; do not silently accept
other conflicting dependencies. The compiled component and complete relocated
app/archive/Designer gates pass at 197cd2a; matching exported symbols is not a
general semantic guarantee for arbitrary provider substitutions.

```sh
# Both SDKs must already be built; output/archive must be fresh.
python3 tooling/package_linux.py \
  --build build/linux-renderer --mite-build build/linux-package-mite \
  --app build/designer-app --output build/linux-designer-candidate \
  --name web-component-designer --version 0.1.0-dev.0 --archive
```

The example assumes the pinned Designer was compiled/bundled with
`tooling/build_designer.py`. It describes staging; native editing, Monaco and
window-control acceptance are supplied by the native gate below at 197cd2a.

The default modes are --dependencies production and --symbols stripped. Use
--symbols keep for native diagnostic symbols. Bundled Designer dependency mode
must match its build metadata; rebuild the source bundle to change it. Other
installed npm trees need a lockfile. The source payload/builds are copied, never
stripped or patched in place. An optional --entry must name an existing app-relative
file; otherwise package.json main or main.mjs is used.

## Layout and size

| Path | Purpose |
| --- | --- |
| launch | Native entry point; execs Firefly and forwards arguments |
| bin/firefly | Actual embedded-Mite main process |
| bin/qt.conf | Package-relative Qt plugin lookup |
| app/ | Application sources/bundles and selected npm payload |
| firefly-runtime/bin/ | Physical owned WebContent and pinned service helpers |
| firefly-runtime/share/ | Pinned renderer resources |
| lib/ | One shared ELF closure for main/helpers/plugins/launcher |
| plugins/ | Dynamic Qt plugins, when used by the SDK |
| licenses/ | Project, Ladybird, JS, vcpkg, Rust and distribution notices |
| BUILD.json | Source/version, symbols, inputs, library hashes and host ABI |
| SHA256SUMS.json | Complete packaged file inventory and checksums |

The standalone Mite CLI is used to check independent linkage and collect notices;
it is not duplicated inside the app. Identical library contents use relative
aliases. All owned ELF objects use package-relative RUNPATHs and are checked after
stripping. Dynamic Qt requires a packaged xcb platform plugin. Distribution DSOs
require native package ownership/copyright provenance. Unknown dependencies fail.

--archive adds an adjacent .tar.gz with one name/version/platform root and a
.sha256 file. Launch the extracted root's launch executable from any directory.
The launcher resolves /proc/self/exe and execs the app, preserving PID and renderer
parent ownership. It defaults to xcb while preserving explicit caller settings,
which the runtime validates. It neither starts an X server nor installs packages.

## Required acceptance and remaining limits

The renderer workflow retains all existing native engine, isolation, process,
sandbox, window and unarchived-app gates. It additionally builds standalone Mite,
stages/strips/archives the actual app fixture, verifies the archive digest, extracts
under spaces/Unicode, verifies every packaged checksum and ELF closure, and runs
that extracted launcher in a dedicated Xvfb/Openbox environment. A live independent
HTTP peer checks real main/renderer executables, parent PID and kernel sandbox
state. It also checks /proc maps for ELF files loaded from either original SDK.
A compiled launcher fixture separately tests forwarding, PID ownership, relocation,
platform defaults/refusals and actual distribution copyright collection. This
component gate passes at `da5e3e8` in
[run 42349719941](https://github.com/jogibear9988/elytra/actions/runs/42349719941),
job 128335067273. Its synthetic runtime does not establish Firefly app acceptance.

The renderer workflow additionally compiles/bundles the pinned production Designer
and runs `test_linux_designer_package.py` on a fresh isolated display. This driver
uses the same archive extraction/hash/ELF checks, then runs the existing shared
Designer server/security/edit/Monaco/HTML controls/native minimize/close assertions
against the extracted app and launcher. All existing macOS assertions remain
required; Linux has no reduced Designer acceptance definition.

```sh
FIREFLY_TEST_ISOLATED_X11=1 QT_QPA_PLATFORM=xcb xvfb-run -a \
  -s '-screen 0 1280x900x24' \
  python3 tooling/tests/test_linux_designer_package.py \
  --build build/linux-renderer --mite-build build/linux-package-mite \
  --app build/designer-app --version 0.1.0-dev.0
```

Only a successful driver copies the original candidate archive, SHA-256 and
BUILD.json to build/linux-designer-packages. CI uploads that directory as
linux-designer-package-candidate with missing files treated as an error. Failures
retain staging/acceptance/window-manager metadata under the diagnostics artifact.
This is an experimental Actions candidate, not automatic Linux release publication.

These gates pass on the committed source above; later sources need fresh gates.
Release publication remains separate from the accepted Actions candidate.
Compatible host glibc/loader, X11 and host
GPU/audio drivers/services remain requirements. Wayland, other architectures,
older glibc/distributions, installer/signing/update lifecycle and complete security
containment are not established. See [ADR 0094](../architecture/0094-linux-elf-package-closure.md).
