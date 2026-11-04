# ADR 0001: pinned upstream and subsystem boundaries

Status: accepted for the first vertical slice, 2026-11-04.

## Evidence

Inspected Ladybird commit `51ef0d743ceca25863847f8cea208c1c6f7031ce`
(`vendor/ladybird`, an unmodified Git submodule):

- `Libraries/LibJS/CMakeLists.txt`: public C++ API over a Rust runtime/embedding
  ABI. `LibJSStandalone` is the intended non-LibWeb link target.
- `Libraries/LibJS/Runtime/{VM,Realm}.h`, `Script.h`: VM creation, host realm,
  script parsing and execution hooks; these are version-specific internal APIs.
- `Libraries/LibJS/Rust/{Cargo.toml,build.rs}`: Rust 2024 runtime; generated
  embedding layouts and architecture-specific interpreter assembly.
- `rust-toolchain.toml`: Rust 1.98.0. `Meta/CMake/compile_options.cmake`: C++23.
- `Libraries/LibWebView/{ViewImplementation,WebContentClient}.h` and
  `Services/WebContent`: rendering is a process protocol, not an embeddable
  browser widget with a stable public ABI. Qt is the desktop upstream UI.
- `Documentation/BuildInstructionsLadybird.md`: Linux/macOS builds, experimental
  native clang-cl Windows builds; WSL2 is the supported Windows browser route.
  MinGW/MSYS2 is explicitly unsupported. Some utility targets have WIN32 guards.
- Upstream root CMake configures graphical dependencies even with GUI targets
  disabled. Mite needs a reduced dependency configuration, not a LibWeb dependency.
- `LICENSE`: BSD-2-Clause, with separate third-party licenses. Preserve upstream
  notices and ship dependency notices with eventual binary distributions.

## Decisions

Use C++23 for Mite's host integration, reusing LibJS's C++ ownership and GC APIs.
Retain the upstream Rust implementation without adding a second hand-written FFI.
Statically link a reduced set of upstream library directories, keeping our build adapter
outside the submodule. Pin all fetched sources and fail on missing dependencies. The default vcpkg
triplets provide static dependencies; static upstream libraries also avoid
upstream shared-library installation assumptions about those dependency targets.
A headless Mite build must never include LibWeb, Qt or Firefly.

Use a single owning VM/thread initially. Host callbacks enter that VM only on its
own thread. The eventual loop drains promise jobs between callbacks; I/O workers
post completion data, never manipulate JS heap values. Timers/I/O and unhandled
rejections require behavioral tests before claiming Node ordering compatibility.

Module loading will use canonical per-VM module identities and a separate Node
resolver (ESM URL identities, package boundaries/exports, CJS cache). Do not reuse
browser import resolution as if it were Node resolution.

Firefly will use native windows plus LibWebView/WebContent process integration.
Renderer IPC must use bounded, validated messages, explicit channels and sender
identity; preload capabilities live in a separate realm. Untrusted content gets
no process/filesystem capabilities. A renderer proof of concept must precede broad
Electron API implementation.

## Native Windows strategy and unresolved work

Start from upstream's clang-cl/Win32 adapters, using interpreter mode where the
JIT is unavailable. Compile Mite first on a native Windows runner. Audit filesystem,
sockets, process creation, shared-memory handles and sandboxing separately. For
Firefly, adapt the upstream Qt desktop view/process transport on Windows before
promising a BrowserWindow implementation. Implement missing platform operations
behind small adapters; do not substitute WSL, Chromium, V8 or Node. Windows is
**unverified** until native build and runtime tests pass. Renderer sandboxing and
packaging are separate acceptance gates on every OS.

## Consequences

We own maintenance of the reduced build graph and integration with changing LibJS
APIs. This avoids compiling an entire browser for a CLI runtime while preserving
the engine implementation. Native Windows and renderer isolation are substantial
implementation requirements, not completed work in this ADR.

Reference: https://github.com/LadybirdBrowser/ladybird/tree/51ef0d743ceca25863847f8cea208c1c6f7031ce
