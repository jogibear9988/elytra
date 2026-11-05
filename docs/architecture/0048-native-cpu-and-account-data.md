# 0048: Copy native CPU and account snapshots into LibJS-owned values

Status: node:os extension tested on macOS ARM64. Native Windows/Linux, complete OS
constants/network/priorities and privileged execution remain open.

## Decision

Extend our existing C++ libuv OS adapter with cpus and userInfo queries. Use C++
where the native allocation ownership and LibJS GC roots meet; reuse our existing
Buffer module for JS-facing account encodings. No new dependency, FFI, Node/V8,
LibWeb coupling or Ladybird source modification.

uv_cpu_info allocations are released with a ScopeGuard after copying each model,
speed and user/nice/sys/idle/irq millisecond counter into fresh JS objects/arrays.
Models use the reference's Latin-1 byte interpretation. Root the array, current
record and times object during allocation. Unavailable native CPU information
returns an empty list as in Node, rather than fabricated CPU records. The native
failure branch itself was not forced on this machine.

uv_os_get_passwd supplies current account data independently of environment HOME.
Copy username/homedir/shell bytes into VM-owned Uint8Arrays before releasing the
passwd allocation with its matching libuv free function, including abrupt exits.
JS builds a fresh null-prototype uid/gid/username/homedir/shell result. Supported
encodings use Buffer conversion; encoding=buffer returns independent Buffers.
Nullable shell remains null. Native syscall errors use the existing SystemError
path. Windows -1 ID representation is coded but has not been validated natively.

Option semantics follow observed Node 24.15.0: non-object options (including
functions) are ignored; read an object/proxy's encoding once, without coercing
non-string encodings. Preserve getter exceptions. Encoding aliases ignore case,
unknown labels fall back to UTF-8, and the portion before NUL determines the
encoding. This is deliberately different from Buffer.toString's invalid-label
error, so validate/fallback before calling the shared Buffer implementation.

## Evidence

The added mite-os-data differential suite compares 49 account encoding/option/
identity/ownership observations with Node: null prototype and property order, string
and Buffer output, aliases/case/NUL/unknown labels, primitive/callable/inherited
options, getter/proxy reads, thrown-value identity, revoked proxies, HOME independence,
fresh results/byte buffers and CJS/ESM named function identity.

Fourteen actual CPU records match the reference's model list. Per-record shapes,
prototypes, numeric speed/counters, monotonic counters within and between the ordered
queries, ignored arguments/receiver and independent mutable result graphs pass.
Speed/counter samples are live observations, not simultaneous equality claims or a
performance benchmark. All 29 standalone CTest suites pass in build/os-data-ctest.log.
Firefly's native main-process ESM/HTTP fixture imports and executes cpus/userInfo;
its full existing Electron reference gate passes in build/os-data-firefly-test.log.
The prior OS suite still passes, including explicit remaining unsupported APIs.

## Limits

Network interfaces, priorities and constants still reject explicitly. Native
Windows/Linux, account database/syscall failures, non-ASCII real account/model data,
CPU hotplug and privileged/setuid/capability execution were not exercised. Byte
encoding uses the separately tested Buffer implementation, but no complete Node
metadata/error/monkey-patched-intrinsics parity is claimed. Native query blocking
matches the synchronous API shape; no worker/async wrapper is implied. No new npm,
packaging, renderer bridge, full audit or visible-desktop support is claimed.
Existing build warnings remain and Ladybird's pin/source are unchanged.

Reference: official Node v24.15.0 [native OS binding](https://github.com/nodejs/node/blob/v24.15.0/src/node_os.cc),
[OS JS wrapper](https://github.com/nodejs/node/blob/v24.15.0/lib/os.js) and
[encoding parser](https://github.com/nodejs/node/blob/v24.15.0/src/api/encoding.cc).
Only behavior was consulted; these implementation files are not embedded.
