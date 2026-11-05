# 0047: Add native OS observations to standalone Mite

Status: partial node:os, validated on macOS ARM64. Other native platforms and the
remaining OS APIs are unverified/unimplemented.

## Decision

Use the existing libuv dependency for operating-system queries, with a small C++
LibJS adapter and our own JS module for Node-facing exports/coercion/descriptors.
No additional FFI, Node/V8, renderer dependency, upstream patch or npm package.
Register os/node:os in the normal builtin cache and ESM shim graph; the common
runtime CMake graph also makes it available to Firefly main-process hosts.

Implement arch/platform, type/release/version/machine, hostname/homedir/tmpdir,
endianness, availableParallelism, freemem/totalmem, uptime/loadavg, EOL and devNull.
OS information strings are captured at module initialization from uv_os_uname;
other observations use their native query when called. loadavg produces a fresh
JS array. JS wrappers ignore unused arguments/receivers and expose the measured
Symbol.toPrimitive behavior, retaining original function identity on coercion even
when an export property is replaced. EOL/devNull are enumerable, nonwritable and
configurable. arch/platform read the process global as in the reference module.

Temporary-directory precedence and removal of one trailing separator follow the
reference's platform branches. Read the actual native environment each time,
independently of a replacement process.env JS object. POSIX homedir uses a PATH_MAX
buffer, matching the observed Node ENOBUFS boundary rather than silently expanding
it. Environment reads for tmpdir grow their native buffers. Native failures throw
SystemError/ERR_SYSTEM_ERROR with uv error information; exact accessor descriptors,
stack formatting and every failure path are not claimed compatible.

cpus/networkInterfaces/userInfo/getPriority/setPriority explicitly throw
ERR_MITE_UNSUPPORTED_API. Reading the default object's constants property throws
that code too; constants is not advertised as an ESM named export. There is no
empty success-returning constants object. Broadening these APIs remains required.

## Validation

The OS differential fixture compares the installed Node 24.15.0 with native Mite:
fixed system values, 15 argument/receiver checks, 12 primitive coercions, retained
function coercion, CJS/bare/node:/ESM identities, named imports, EOL/devNull
descriptors, fresh/nonnegative load arrays and native memory/uptime bounds. Uptime
is compared within the measured invocation interval; changing free-memory/load
values are validated by shape/range, not falsely compared as simultaneous samples.

Fifteen live environment observations cover precedence, empty values, root/double/
trailing separators, relative paths, Unicode, an 8 KiB temporary path, HOME changes,
the PATH_MAX failure and replacement process.env objects. Child fixtures restore
their native environment. Separate Mite checks require explicit errors for the six
unsupported API/property groups. All 28 standalone CTest suites pass in build/os-ctest.log; the native Firefly/
Electron regression gate passes in build/os-firefly-test.log. Its ESM/HTTP fixture
executes OS named imports in the embedded main runtime before loading the renderer.
otool still reports only platform libraries, with no Node/V8/LibWeb/Qt dependency.

## Limits

CPU/network lists, user information, priorities and OS constants are still absent.
Function names/internal descriptors, all system-error accessors/failures and coercion
edge cases are not comprehensively compatible. Privileged/setuid/capability execution
and Node's credential-specific environment filtering remain unverified; no hardened
privileged-host support is claimed. The Windows lexical branch/fallback buffer and
Linux behavior need native runners. This does not provide node:os platform support
beyond the tested macOS host or solve Firefly preload/IPC, packaging or other Node
builtins. Existing Rust archive/deployment warnings remain. Ladybird stays at the
same unmodified pin.

Reference behavior was inspected in the official Node v24.15.0
[JS OS module](https://github.com/nodejs/node/blob/v24.15.0/lib/os.js),
[native OS binding](https://github.com/nodejs/node/blob/v24.15.0/src/node_os.cc) and
[temporary-directory environment lookup](https://github.com/nodejs/node/blob/v24.15.0/src/node_credentials.cc).
Our adapter embeds none of those implementation files.
