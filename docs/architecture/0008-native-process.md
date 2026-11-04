# ADR 0008: native process basics

Status: implemented subset, tested on macOS ARM64 against Node v24.15.0.

Use libuv for cwd/chdir, process IDs, executable path and environment access.
Keep platform/architecture compilation branches in Process.cpp. Build the JS
process object before module loading, normalize argv[0] to the actual executable
path and preserve argv0. The builtin registry retains the original process object
so replacing globalThis.process cannot impersonate node:process.

Implement process.env as a JS Proxy over private native operations. Reads,
assignment, deletion, enumeration and property descriptors reflect OS environment
state; validate/coerce values before crossing into libuv and release its owned
buffers. Native calls occur on the owning VM thread. Tests use only named fixture
variables, including long Unicode values, and never print the host environment.
NUL arguments are rejected before native calls. Full legacy coercion/deprecation
and prototype edge cases are not claimed.

Validate exitCode as Node-style integer/string input and store its int32 value.
Let native handles drain before applying an exitCode; process.exit terminates
immediately through the native C runtime. An explicitly selected code suppresses
the unsettled-top-level-await warning, matching the executable reference. Exit,
beforeExit, uncaughtException and process rejection events are not implemented;
process EventEmitter methods, stdio streams, signals and nextTick remain absent.

Report the real Mite runtime identity: version v0.1.0, release.name mite, versions
for Mite/libuv/pinned LibJS, and no versions.node. Pretending that the runtime is
Node v24 would misrepresent unsupported host features. This is an intentional
compatibility difference, tested separately from shared Node fixtures.

Expose supported methods/data through the same node:process object for global,
CommonJS and ESM access. Named builtin exports still have the snapshot semantics
and syncBuiltinESMExports limitation recorded in ADR 0007. Native Windows/Linux
execution is still required; successful macOS tests do not establish those ports.

References: [Node process](https://nodejs.org/api/process.html),
[libuv OS utilities](https://docs.libuv.org/en/v1.x/misc.html).
