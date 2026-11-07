# ADR 0094: Linux ELF package dependency closure

Status: native component acceptance passes at `209f64e` in
[run 42348028753](https://github.com/jogibear9988/elytra/actions/runs/42348028753), job 128329923480, on Ubuntu 24.04 x64
with Clang 22.1.8/patchelf 0.18.0. Full experimental Firefly archive/extracted
Designer gates now pass at 197cd2a in
[run 42379262065](https://github.com/jogibear9988/elytra/actions/runs/42379262065).
Broader distribution/installer/update support remains unfinished.

## Context

The scoped native X11 app can find its owned renderer after relocation. Its
Linux ELF libraries still resolve from the build tree, so helper relocation is
insufficient to establish a distributable. Mite, Firefly and helpers can share
large dependencies; copying each dependency closure independently wastes space.

## Decision

Use the existing Python build-tool layer for filesystem staging and provenance.
Invoke native ldd on trusted build outputs and patchelf for RUNPATH rewriting.
No runtime FFI, alternate JavaScript engine or Ladybird patch is introduced.
The component currently refuses hosts other than native Linux x64.

Callers copy main/helper/plugin ELF objects into a fresh package and pass exact
destination/original mappings. Resolve dependencies at the original location:
existing build RUNPATHs can already contain $ORIGIN and cannot resolve from an
incomplete destination tree. Require matching hashes and separate physical
copies, rejecting hardlinks before any mutation. Leave original ELF files intact.

Collect transitive dependencies once in a common lib/ directory. The same SONAME
with different contents fails before copying/patching; identical payload aliases
use relative local symlinks. Keep the explicit host glibc/loader ABI external.
The inventory records actual external ABI names, source and packaged hashes,
original binary paths, tool versions and physical library count.

| Package object | Expected RUNPATH |
| --- | --- |
| bin/firefly | $ORIGIN/../lib |
| firefly-runtime/bin/WebContent | $ORIGIN/../../lib |
| lib/shared-library.so | $ORIGIN |

Verify every patched object with loader environment overrides removed. Resolved
non-host-ABI dependencies must come from this package's lib/ tree. A missing
library must fail rather than succeed through a developer search-path override.

## Validation and limits

The native fixture builds an actual transitive DSO chain, stages main/helper
layouts sharing those libraries, checks deduplication/source preservation,
moves the package, removes the original build paths and executes both binaries.
It tests relative and absolute build RUNPATH discovery, missing packaged DSOs,
conflicting contents, hardlinks, changed copies and existing-output refusal.
Parser/platform refusal checks run locally without ELF-loader execution.

A dedicated package-component workflow must run independently of the long Mite
and renderer workflow concurrency groups. The renderer workflow also keeps this
component gate before building the actual application; all existing native and
sandbox gates remain required. This fixture does not prove Firefly packaging.

The experimental [application packager](../development/linux-packaging.md) adds
resources, explicit dynamic Qt plugins, dependency notices, a launcher/archive
format and an actual extracted Firefly fixture gate. Native whole-app acceptance
of that implementation and Designer packaging/UI acceptance remain required. Host GUI/audio/driver/plugin and glibc requirements
need explicit platform coverage. No arbitrary distribution, older ABI, other
architecture, Linux release asset or complete security containment is claimed.
