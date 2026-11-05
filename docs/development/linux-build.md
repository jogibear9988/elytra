# Native Linux bring-up

The Native Linux development workflow builds standalone Mite on Ubuntu 24.04
x64 with Clang 22, pinned Rust 1.98.0 and the pinned vcpkg source/dependencies.
It runs the same enabled CTest suites and locked npm fixtures as the other native
runtime jobs, then the CLI examples and ldd. Tests are not skipped or replaced
with a Node execution. Node is only the differential reference/build tool.

**Status: workflow implemented; first native build and behavior results pending.**
This is not a Linux support announcement or a Firefly desktop/package gate.
There is no Linux GUI dependency in the standalone Mite source graph.

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

The workflow retains CMake, Cargo, vcpkg and CTest logs even after failures.
Dependency caches use individual ABI keys and unique saves so partial build
downloads can accumulate; a restored cache never substitutes for compilation or
tests. An actual hosted result must establish compilation, filesystem/network/OS
behavior, linkage and CLI operation. Investigate failures against native Node
observations without weakening tests to advertise unsupported behavior.

Firefly Linux windows, renderer helper/sandbox integration and distributable
packages remain separate implementation and native acceptance work. See the
[renderer build](renderer-build.md), [support policy](../reference/support-policy.md)
and [progress record](../PROGRESS.md).
