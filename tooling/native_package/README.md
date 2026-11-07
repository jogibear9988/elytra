# Native package components

These are private build-time helpers, separate from runtime code. They do not
embed Node/V8, modify Ladybird or install dependencies on application startup.

## ELF library closure

elf.py stages a trusted native Linux x64 build's resolved shared-library closure.
Callers first copy their executables/helpers into a fresh package tree, then call
stage(root, {destination: original, ...}). Originals must be separate files outside
the staged tree; staged copies must match their source hashes. Dependency discovery
runs at the original location, supporting both absolute and $ORIGIN build RUNPATHs.
Only the staged copies are patched. The component collects dependencies with the
native loader, rejects unresolved libraries or same-name/different-content
collisions before mutation, and copies dependencies once into lib/. Identical
payload aliases are local relative symlinks. Original build files are not patched.

Each staged executable/helper/library receives a relative $ORIGIN RUNPATH.
verify(root, objects) checks that the loader resolves every non-host-ABI library
from this package. Environment LD_* overrides are removed for inspection. The
returned inventory records source and packaged hashes, tool versions, deduplicated
library count and actual external host ABI names. glibc/the host loader remain
external; arbitrary Linux distributions and other architectures are not accepted.

This component does not yet create a Firefly app distribution. The application
packager still needs resources, explicitly loaded plugins, dependency licenses,
launchers, archives and an actual relocated Firefly/Designer acceptance gate.
Do not mistake the compiled fixture for acceptance of those deliverables.

On native Linux x64 with a C compiler and patchelf:

```sh
python3 tooling/tests/test_elf_package.py
```

The fixture builds real transitive DSOs and two executable layouts, checks shared
library deduplication/source preservation, moves the package, removes the original
build paths and executes both programs. Missing libraries, content collisions and
unsafe staging inputs must fail. This native gate runs in an independent Linux package-component workflow and before the large
renderer build. Native [run 42348028753](https://github.com/jogibear9988/elytra/actions/runs/42348028753)
at `209f64e` passes on Ubuntu 24.04 x64 with Clang 22.1.8 and patchelf 0.18.0:
real relocation/deduplication, both executions and refusal cases succeed.
This is component acceptance, not a Linux app package.

For host/parser refusal checks only, without loader execution or GUI:

```sh
python3 tooling/tests/test_elf_package.py --check-guard
```

## Shared dependency notices

notices.py preserves project/Ladybird notices, verified embedded JavaScript
licenses, vcpkg copyright records and pinned Rust package metadata/notices. Crates
without standalone notice files retain their source archive. The existing macOS
packager imports this same implementation; passing one combined runtime SDK
avoids copying its vcpkg notice set twice. OS distribution library notices remain
a separate Linux packaging responsibility.
