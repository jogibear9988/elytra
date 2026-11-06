# Mite and Firefly

[Project website](https://jogibear9988.github.io/elytra/) ·
[Documentation](docs/index.md) · [Getting started](docs/getting-started.md) ·
[API reference](docs/api/index.md) · [Roadmap](docs/roadmap.md)

Work in progress: a LibJS-powered Node-compatible runtime and a Ladybird-powered
Electron-compatible desktop framework. The implementation requirements are in
[mite-firefly-agent-instructions.md](mite-firefly-agent-instructions.md).

Mite runs a tested subset of Node-style CLI, module, timer, Buffer and filesystem behavior on macOS
ARM64, with [native Linux](docs/development/linux-build.md) and
[Windows x64](docs/development/windows-build.md) also passing the documented runtime suites.
Firefly now runs a first native macOS app/BrowserWindow slice on Mite,
with a shared Electron lifecycle fixture. General compatibility and native
Windows/Linux desktop validation remain work in progress. See the
[Firefly development build](docs/development/renderer-build.md) and
[compatibility matrix](docs/compatibility/firefly-0.1.md).
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

The first [macOS ARM64 alpha](https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.1)
includes native Mite, Firefly and the Designer app. It is an ad-hoc signed,
not-notarized snapshot at 745c7af; current master contains newer work. Windows/Linux
desktop packages remain in progress. See the release notes for its exact scope.

## Development build

Requires Git, Python 3.9+, Ninja, CMake 3.30+, Rust 1.98.0 (rustup), a C++23 compiler
and native platform build tools. Node and npm are required for differential tests and regenerating the checked-in
portable JavaScript libraries; they are never runtime dependencies. See the
[runtime library update instructions](tooling/runtime-js/README.md).
macOS needs Xcode and `pkg-config`, autoconf, autoconf-archive, automake and libtool. Dependencies
are built locally using a pinned vcpkg checkout; the first build downloads and
compiles ICU, OpenSSL and the LLVM libc math headers and can take several minutes.

```sh
python3 tooling/bootstrap.py
cmake --preset dev
cmake --build --preset dev --parallel 4
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/npm-fixtures
ctest --preset dev
build/dev/bin/mite examples/mite-cli/hello.mjs world
build/dev/bin/mite examples/mite-cli/modules.mjs world
build/dev/bin/mite examples/mite-cli/hash.mjs
```

On native Windows, use an x64 Visual Studio developer terminal with clang-cl and
Ninja; set `CC=clang-cl` and `CXX=clang-cl` in that terminal before configuring.
Windows runtime acceptance remains pending. The [native Windows bring-up guide](docs/development/windows-build.md)
records the CI path, actual setup evidence and remaining gates. This project does not use WSL as a runtime
or build fallback. The [native Linux bring-up guide](docs/development/linux-build.md)
records the passing Ubuntu/Clang baseline and the scope still requiring validation.
Firefly and npm compatibility are still under development; see
[Mite coverage](docs/compatibility/mite-0.1.md) and
[Firefly coverage](docs/compatibility/firefly-0.1.md).

Mite also provides native `node:os` system observations, including hostname,
platform/machine, home/temp directories, memory, uptime, parallelism, CPU snapshots
and user information, plus IPv4/IPv6 network-interface snapshots. Native process
priority queries/setters and OS constant groups are also available. Native validation
currently covers macOS ARM64; see the
[Mite compatibility matrix](docs/compatibility/mite-0.1.md).

`node:crypto` currently provides OS-backed `randomBytes()` and UUIDv4
`randomUUID()`, plus sync/callback `randomInt()` with unbiased bounded selection.
The [random CLI example](examples/mite-cli/random.mjs) uses all three;
incremental `createHash()`/`Hash` also support independent copies and Transform
streams. The [hash CLI example](examples/mite-cli/hash.mjs) hashes text or a file.
Native createHmac/Hmac and timingSafeEqual additionally support message authentication
and equal-length byte comparison. Ciphers, KeyObject/CryptoKey, asymmetric signatures
and Web Crypto remain unimplemented. Hash/provider and
platform limitations are recorded in the compatibility matrix and ADR 0053.

`node:child_process.spawn()` now launches native executables with real pipes and
process lifecycle events. The [CLI example](examples/mite-cli/spawn.mjs) starts
another copy of the current runtime. See the [process reference](docs/api/child-process.md)
for options and the still unsupported shell/IPC/synchronous APIs. Callback-based
`execFile()` adds buffered text/binary output, per-stream limits and timeout;
the [buffered execution example](examples/mite-cli/exec-file.mjs) runs in Node and Mite.

Measured real-package scenarios are recorded in the [npm compatibility matrix](docs/compatibility/npm-packages-0.1.md). Missing test dependencies cause a test failure with installation instructions.

The [filesystem example](examples/mite-cli/files.mjs) copies its source using
`node:fs/promises`. Pass a destination file that does not yet exist:

```sh
build/dev/bin/mite examples/mite-cli/files.mjs build/dev/files-example-copy.mjs
```

The [TCP echo example](examples/mite-tcp/echo.mjs) listens on numeric loopback;
connect using an ordinary TCP client. Pass `0` to choose a free port:

```sh
build/dev/bin/mite examples/mite-tcp/echo.mjs 3000
nc 127.0.0.1 3000
```

TCP supports IPv4/IPv6 addresses and OS hostname lookup, including custom lookup
functions and partial automatic family retry. node:dns lookup and its Promise API
are available; DNS record-query APIs and TLS remain open.

The [HTTP server example](examples/mite-http/server.mjs) serves JSON and streams
POST /echo through the request/response streams:

```sh
build/dev/bin/mite examples/mite-http/server.mjs 3000
curl http://127.0.0.1:3000/hello
curl --data-binary 'hello' http://127.0.0.1:3000/echo
```

HTTP support is partial. The [client example](examples/mite-http/client.mjs) uses
native request/get streams; see the [HTTP reference](docs/api/http.md) for options
and errors. Client pooling and HTTPS/fetch remain open.

## Web Component Designer sample

The [pinned Designer sample](examples/web-component-designer/README.md) builds
node-projects/web-component-designer-demo and runs it in Firefly with a Mite main
process. Its macOS ARM64 smoke test checks app boot, document editing and Monaco.
Native Windows packaging and broader designer feature validation remain open.

## Website and release infrastructure

The [project website](https://jogibear9988.github.io/elytra/) and API reference deploy
from repository sources through GitHub Pages. See [website development](docs/development/website.md).
The [native release workflow](docs/development/releases.md) builds and tests both
projects and the Designer sample before packaging/uploading a draft prerelease.
Required native Windows support is still missing and blocks a complete release;
macOS bundles are experimental and ad-hoc signed, not notarized.
