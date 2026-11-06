# Runnable examples

Commands run from the repository root after a successful source build. Replace
build/dev/bin/mite with your actual executable location. Current native evidence is
macOS ARM64. Authored examples use modern ESM; CommonJS remains supported and
covered by dedicated compatibility fixtures. A runnable example does not establish full module/platform compatibility.

| Example | Command / prerequisite | What it demonstrates |
| --- | --- | --- |
| [Basic CLI](../../examples/mite-cli/hello.mjs) | `build/dev/bin/mite examples/mite-cli/hello.mjs world` | Script entry, console and arguments |
| [Native hashing](../../examples/mite-cli/hash.mjs) | `build/dev/bin/mite examples/mite-cli/hash.mjs` | ESM imports and native hash state |
| [Modules](../../examples/mite-cli/modules.mjs) | `build/dev/bin/mite examples/mite-cli/modules.mjs` | ESM/JSON imports and timer-driven await |
| [File copy](../../examples/mite-cli/files.mjs) | Pass a destination that does not exist | Promise filesystem I/O and exclusive creation |
| [Streams](../../examples/mite-stream/pipeline.mjs) | `build/dev/bin/mite examples/mite-stream/pipeline.mjs` | In-memory pipeline; no filesystem stream claim |
| [TCP echo](../../examples/mite-tcp/echo.mjs) | Pass a port; connect with a TCP client | Native socket streams and loopback listening |
| [HTTP](../../examples/mite-http/server.mjs) | Pass a port; send requests with curl | JSON and streamed POST echo |
| [Random](../../examples/mite-cli/random.mjs) | `build/dev/bin/mite examples/mite-cli/random.mjs` | Native random bytes and UUIDv4 |
| [Hash](../../examples/mite-cli/hash.mjs) | Optional input filename | Native incremental hash and independent copy |
| [Native desktop](../../examples/firefly-hello-world/main.mjs) | Build Firefly, run `python3 tooling/build_hello.py`, then launch `build/firefly-hello-world/main.mjs` | Native window, file load, renderer PID and close/quit |
| [Designer](../../examples/web-component-designer/README.md) | Build pinned demo with Node/npm as build tools | Actual app boot, document edit and Monaco through Firefly |

The [test tooling](../../tooling/) adds Node/Electron differentials, independent
network peers and security-boundary fixtures. Test-only endpoints and compiled SDK
probes are not application APIs. [npm measurements](../compatibility/npm-packages-0.1.md)
record exact published package versions and the narrow workflows verified.

When adding an example, include its runtime/build prerequisites, a deterministic
success observation, cleanup/failure behavior and a verification command. Do not add
an example for an unsupported API merely to match an upstream tutorial category.
