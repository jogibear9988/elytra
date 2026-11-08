# JavaScript compatibility modules

These are maintained ESM sources, grouped by the service they implement. They are
not application CommonJS modules. Each default factory receives a realm-local
native host and returns the shared builtin object used by application ESM imports
and CommonJS require. Native handles, threads and OS calls stay in
[the native runtime](../runtime/README.md).

| Directory | Sources and responsibility |
| --- | --- |
| core | Events, buffers, lexical paths, process properties, URL/text facades and nonterminal readline |
| network | DNS overloads, [TCP socket/listener subsystems](network/net/README.md) and [HTTP client/server subsystems](network/http/README.md) |
| filesystem | Filesystem overloads, callback/Promise adaptation, FileHandle lifetime and bounded native read streams |
| compression | Native-backed zlib Transform streams, flush ordering, bounded convenience output and explicit option policy |
| crypto | Assembly, OS entropy APIs, digest/HMAC lifecycle and byte adaptation |
| process | ChildProcess streams, native completion and bounded execFile collection |
| system | OS metadata/constants and argument validation |
| generated | Reproducible embedding output and third-party notices; edit its inputs instead |

Factories prevent accidental process-global state and resolve builtin bootstrap
cycles explicitly. Native dependency injection is internal, not a public way for
applications to replace OS or crypto services. Constructor functions that remain
callable deliberately preserve Node APIs; replacing them with classes would change
observable behavior.

A synchronous CommonJS require and initial process/timer bootstrap cannot await an
ESM evaluation cycle. The [generator](../../tooling/runtime-js/build.mjs) bundles
private ESM imports and emits a synchronous factory adapter for LibJS. Normal
native builds check committed source/output hashes and require no Node/npm runtime.
See [generation instructions](../../tooling/runtime-js/README.md).

Keep validation before native work, explain resource ownership and non-obvious
ordering next to state transitions, and use descriptive helper names. Tests must
exercise the real runtime and the Node reference; controlled native substitutes
belong only in tests. Add API/matrix entries alongside behavior changes.
