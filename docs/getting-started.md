# Getting started

Mite is a standalone JavaScript runtime using LibJS. Firefly adds native desktop
windows and Ladybird rendering, with Mite in the main process. Neither embeds Node
or V8. Both projects are in early development; build from source for now.

Native tests cover macOS ARM64 and a scoped [Linux x64 Mite baseline](development/linux-build.md).
Windows runtime acceptance, Windows/Linux desktop adapters, signing and distributable
packages are still being implemented. No WSL or alternate
browser engine is used as a fallback.

## Build Mite

Install Git, Python 3.9+, Ninja, CMake 3.30+, Rust 1.98.0 through rustup, a C++23
compiler, and your platform's native tools. macOS also needs Xcode, pkg-config,
autoconf, autoconf-archive, automake and libtool. Node and npm are development/test tools only.
Dependencies are pinned and built through vcpkg; allow substantial disk space and
time for the first build.

```sh
git clone --recurse-submodules https://github.com/jogibear9988/elytra.git
cd elytra
git checkout master
git submodule update --init --recursive
python3 tooling/bootstrap.py
cmake --preset dev
cmake --build --preset dev --parallel 4
build/dev/bin/mite examples/mite-cli/hello.js world
```

For an existing clone, update the branch and initialize its submodules together.
Ladybird must remain unmodified; see the [upstream update procedure](development/updating-ladybird.md).

## Run JavaScript

```sh
build/dev/bin/mite examples/mite-cli/modules.mjs
build/dev/bin/mite examples/mite-cli/hash.mjs README.md
build/dev/bin/mite examples/mite-http/server.mjs 3000
```

The last command starts a server; visit `http://127.0.0.1:3000/hello` or send a
request with curl. `.mjs` selects ES modules. `.cjs` selects CommonJS. For `.js`,
use a nearby package.json with `"type": "module"` for ESM; syntax detection is not
implemented. Existing npm project structures and lockfiles can be used with npm;
Mite does not include a package manager. Check [measured npm compatibility](compatibility/npm-packages-0.1.md)
before relying on a package.

## Run the tests

Install the pinned published test packages with lifecycle scripts disabled:

```sh
npm ci --ignore-scripts --no-audit --no-fund --prefix tooling/npm-fixtures
ctest --preset dev
```

Node is the behavioral reference for these tests, never a runtime fallback.
The current implementation has 35 standalone suites; results and known differences
are recorded in [PROGRESS](PROGRESS.md).

## Build and run Firefly

The renderer is a separate, much larger build using upstream's pinned graphics,
media, Qt and browser dependencies. On the tested macOS host:

```sh
python3 tooling/build_renderer.py --target firefly --jobs 4
build/renderer/bin/firefly examples/firefly-hello-world/main.cjs
```

Use an unlocked graphical session for visible-window tests. The development layout
uses helpers/resources from the companion Ladybird build. It is not yet an installed
application bundle. See the [renderer build guide](development/renderer-build.md)
for the separate renderer, isolation, helper and Electron-reference gates.

## Next steps

Read the [Mite API](api/mite.md), [Firefly API](api/firefly.md) and their compatibility
matrices. [CommonJS preload/contextBridge](api/context-bridge.md) has a partial
implementation. Public IPC, desktop services and native Windows/Linux desktop
acceptance gates remain outstanding; an Electron app cannot yet migrate by changing
only its import in the general case.
