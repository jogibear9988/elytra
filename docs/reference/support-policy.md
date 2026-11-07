# Compatibility and support labels

The specification favors familiar Node/Electron APIs and recorded differences. The
reference versions are Node 24.15.0 and Electron 45.2.3. Mite/Firefly are development
implementations; compatibility evidence is scoped to the tested behavior and host.

## API status

| Label | Meaning |
| --- | --- |
| compatible | The stated contract has supporting tests within its listed scope; it does not certify an entire upstream module |
| partial | Some behavior works and is tested; remaining overloads/options/events or edge cases are listed |
| unsupported | The API/mode is absent or explicitly rejects; no success placeholder is provided |
| intentional difference | A measured difference has an architectural/policy rationale and migration guidance |

Documentation coverage is separate: a page may be an overview, a detailed reference,
or a planned contract. An API inventory or roadmap entry is not an implementation.
Export presence alone does not establish that every mode is supported.

## Platform status

Native macOS ARM64 has working builds and the recorded runtime/renderer tests.
The tested Mite subset passes all 65 enabled suites at 19ad113 on native
[Windows clang-cl x64](../development/windows-build.md) and
[Ubuntu 24.04 Clang 22 x64](../development/linux-build.md). These CLI gates do not validate Firefly desktop or packages. Separately, the
[Linux renderer/application gate](../development/linux-renderer.md) at 19ad113
passes scoped native X11 window, preload/IPC, clipboard and live sandbox-activation
checks under Xvfb/Openbox. It does not certify containment, arbitrary Linux
distributions/desktops, Wayland, optional compiler profiles or Linux/Windows packages. Lexical path tests alone do not
establish native filesystem behavior; use the recorded native suite evidence.

Experimental macOS packages are ad-hoc signed and not notarized. Their minimum OS
version is conservatively the build host; older deployment targets in linker output
do not establish older-host support. Visible desktop presentation requires its own
unlocked-session gate, separately from compositor captures and DOM tests.

## Changes and versions

Current version information is developmental. No stable public compatibility promise,
LTS schedule, support lifetime or release/deprecation history is invented. New behavior
must update reference/compatibility/test evidence in the same logical commit. Published
release notes and API change records should describe actual release artifacts when
those artifacts exist.

Start with the [Mite matrix](../compatibility/mite-0.1.md),
[Firefly matrix](../compatibility/firefly-0.1.md), [npm measurements](../compatibility/npm-packages-0.1.md)
and [PROGRESS](../PROGRESS.md). The matrices describe this repository, not all of
LibJS's language conformance or Ladybird's browser feature coverage.
