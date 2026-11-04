# Mite and Firefly

Work in progress: a LibJS-powered Node-compatible runtime and a Ladybird-powered
Electron-compatible desktop framework. The implementation requirements are in
[mite-firefly-agent-instructions.md](mite-firefly-agent-instructions.md).

Mite runs a tested subset of Node-style CLI, module, timer, Buffer and filesystem behavior on macOS
ARM64. General Node compatibility, Firefly and native Windows/Linux validation
remain work in progress.
See [implementation progress](docs/PROGRESS.md) and the
[upstream integration decision](docs/architecture/0001-upstream-and-boundaries.md).

```sh
git submodule update --init --recursive
```

The project is MIT licensed; Ladybird is BSD-2-Clause with additional third-party
notices retained in the pinned source tree. Binary distributions must also carry
licenses of their linked dependencies.

Ladybird stays an unmodified submodule. Its pin is checked at configure/build/test
time; our adapters carry integration changes. See the
[upstream update workflow](docs/development/updating-ladybird.md) for reviewing
new revisions without maintaining a fork.

## Development build

Requires Git, Python 3.9+, Ninja, CMake 3.30+, Rust 1.98.0 (rustup), a C++23 compiler
and native platform build tools. Node and npm are required for differential tests and regenerating the checked-in
portable JavaScript libraries; they are never runtime dependencies. See the
[runtime library update instructions](tooling/runtime-js/README.md).
macOS needs Xcode and `pkg-config`, autoconf, automake and libtool. Dependencies
are built locally using a pinned vcpkg checkout; the first build downloads and
compiles ICU, OpenSSL and the LLVM libc math headers and can take several minutes.

```sh
python3 tooling/bootstrap.py
cmake --preset dev
cmake --build --preset dev --parallel 4
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/npm-fixtures
ctest --preset dev
build/dev/bin/mite examples/mite-cli/hello.js world
build/dev/bin/mite examples/mite-cli/modules.mjs world
build/dev/bin/mite examples/mite-cli/commonjs.cjs
```

On native Windows, use an x64 Visual Studio developer terminal with clang-cl and
Ninja; set `CC=clang-cl` and `CXX=clang-cl` in that terminal before configuring.
Windows and Linux are not yet verified. This project does not use WSL as a runtime
or build fallback. Firefly and npm compatibility are still under development; see
[Mite coverage](docs/compatibility/mite-0.1.md) and
[Firefly coverage](docs/compatibility/firefly-0.1.md).

Measured real-package scenarios are recorded in the [npm compatibility matrix](docs/compatibility/npm-packages-0.1.md). Missing test dependencies cause a test failure with installation instructions.

The [filesystem example](examples/mite-cli/files.mjs) copies its source using
`node:fs/promises`. Pass a destination file that does not yet exist:

```sh
build/dev/bin/mite examples/mite-cli/files.mjs build/dev/files-example-copy.mjs
```
