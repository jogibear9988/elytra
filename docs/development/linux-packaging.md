# Experimental native Linux app packaging

The native x64 ELF closure component passes its compiled relocation gate at
`209f64e`. This application packager builds on it; whole-application archive
acceptance is a separate, required CI gate. Linux packages are experimental
candidates, not published release assets or a verified portable distribution.
The existing macOS packages and release are described in [releases](releases.md).

## Build inputs and command

Use native Ubuntu 24.04 x64 with the pinned dependencies/compiler, patchelf,
GNU strip, dpkg-query and the offline Cargo sources used by the renderer build.
First build the actual Release runtime using [Linux renderer bring-up](linux-renderer.md)
and a separate Release Mite CLI using [Linux development](linux-build.md).
Mite must be built from the standalone root CMake project. A CLI linked from the
full renderer SDK can acquire LibWeb transitively and is refused by this packager.
No Ladybird patch or alternate JavaScript engine is introduced.

```sh
# Both SDKs must already be built; output/archive must be fresh.
python3 tooling/package_linux.py \
  --build build/linux-renderer --mite-build build/linux-package-mite \
  --app build/designer-app --output build/linux-designer-candidate \
  --name web-component-designer --version 0.1.0-dev.0 --archive
```

The example assumes the pinned Designer was compiled/bundled with
`tooling/build_designer.py`. It describes staging, not proof that the Designer
passes native Linux editing, Monaco or window-control acceptance. Those gates
remain required before publishing this sample for Linux.

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
platform defaults/refusals and actual distribution copyright collection.

These new gates must pass on the committed source; local syntax/refusal tests do
not establish native application acceptance. Designer native UI acceptance and
release upload are still pending. Compatible host glibc/loader, X11 and host
GPU/audio drivers/services remain requirements. Wayland, other architectures,
older glibc/distributions, installer/signing/update lifecycle and complete security
containment are not established. See [ADR 0094](../architecture/0094-linux-elf-package-closure.md).
