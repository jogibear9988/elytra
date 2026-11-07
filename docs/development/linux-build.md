# Native Linux bring-up

Current standalone evidence: all 68 suites pass at 678c85e; newer local macOS
process-event/Code-OSS bootstrap slices await their own native Linux acceptance.
The separate [renderer/package gate](linux-renderer.md) at 197cd2a now accepts an
experimental Ubuntu 24.04 x64 X11 stripped archive and extracted bundled Designer.
Historical CLI baselines below do not supersede this scoped later evidence.

The Native Linux development workflow builds standalone Mite on Ubuntu 24.04
x64 with Clang 22, pinned Rust 1.98.0 and the pinned vcpkg source/dependencies.
It runs the same enabled CTest suites and locked npm fixtures as the other native
runtime jobs, then the CLI examples and ldd. Tests are not skipped or replaced
with a Node execution. Node is only the differential reference/build tool.

**Measured baseline:** [run 42130905759](https://github.com/jogibear9988/elytra/actions/runs/42130905759)
at ff51eab passes native compilation, all 37 enabled runtime suites (14.77s),
the three CLI examples and linkage inspection. Node 24.15.0 is the differential
reference. The ELF links only normal Linux system/runtime libraries, with no
LibWeb, Qt, Node or V8 dependency. This validates the documented Mite subset on
Ubuntu 24.04 x64, not arbitrary Linux distributions or a Firefly desktop/package gate.
There is no Linux GUI dependency in the standalone Mite source graph.

The subsequent [run 42134116536](https://github.com/jogibear9988/elytra/actions/runs/42134116536)
at f1a90a9 also passes native compilation, all **38** suites (20.96s), CLI examples
and linkage checks. This includes the null-deallocation adapter and its SDK
regression under default Release. The explicit size profile remains untested on
Linux; these successes do not certify untested compiler modes or later changes.

## Compiler and prerequisites

The workflow uses the [official LLVM apt repository](https://apt.llvm.org/)'s
Ubuntu Noble Clang 22 branch with a repository-scoped signing key. It follows the
compiler major used by our pinned Ladybird's build instructions. CMake is the
versioned [4.2.0 Python distribution](https://pypi.org/project/cmake/4.2.0/).
The job records the compiler/CMake versions. These are build tools, not shipped
JavaScript runtime dependencies.

After installing the workflow's native prerequisites and selecting Clang 22:

```sh
python3 tooling/bootstrap.py
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DCMAKE_C_COMPILER=clang-22 \
  -DCMAKE_CXX_COMPILER=clang++-22 \
  -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=/usr/bin/ld.lld-22 \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build/linux --target mite --parallel 3
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/npm-fixtures
ctest --test-dir build/linux --output-on-failure
ldd build/linux/bin/mite
```

The unified Rust archive uses the upstream feature graph and a guarded lock
(ADR 0057). Linux links must reject duplicate definitions; do not reintroduce
allow-multiple-definition to make a build pass. Release LTO stays off in this
initial platform gate.

## Diagnostics and remaining work

The later d1c9513 run
[42141507395](https://github.com/jogibear9988/elytra/actions/runs/42141507395)
passes all 40 enabled suites in 18.88s, including native crypto.randomInt and the
independent HTTP-client peer tests. CLI examples, linkage and upstream integrity
also pass. This accepts default Linux Release at that source, not the subsequent
Unicode/module/filesystem path changes, an untested size profile or desktop support.

The workflow retains CMake, Cargo, vcpkg and CTest logs even after failures.
Dependency caches use individual ABI keys and unique saves so partial build
downloads can accumulate; a restored cache never substitutes for compilation or
tests. The initial result establishes the measured compilation, filesystem/network/OS
contracts, linkage and CLI examples; each subsequent revision/profile still needs
its own gate. Investigate failures against native Node
observations without weakening tests to advertise unsupported behavior.

Firefly Linux windows, renderer helper/sandbox integration and distributable
packages remain separate implementation and native acceptance work. The new
[renderer/helper bring-up](linux-renderer.md) has an experimental build/CI path,
with actual native acceptance still pending. See the
[renderer build](renderer-build.md), [support policy](../reference/support-policy.md)
and [progress record](../PROGRESS.md).


The latest [run 42155631610](https://github.com/jogibear9988/elytra/actions/runs/42155631610)
at 745c7af passes all **43** enabled CTests in **32.81 seconds**, native compilation,
CLI examples and linkage inspection. This extends the measured default Release
runtime baseline through canonical file URLs, spawn/execFile and child lifecycle
checks. It does not accept the optional size profile or Firefly Linux desktop.
