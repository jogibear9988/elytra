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
Windows bring-up is ongoing; a successful checkout/compiler setup is not successful
runtime, renderer, sandbox or package validation. Linux remains unverified here.
Lexical Windows path tests on macOS do not establish native Windows I/O behavior.

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
