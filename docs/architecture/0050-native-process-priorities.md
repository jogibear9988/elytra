# 0050: Add native process priority getters and setters

Status: node:os priority operations validated on macOS ARM64. Native Windows/Linux,
privilege escalation/permission-denied paths and full error-message parity remain open.

## Decision

Use existing libuv uv_os_getpriority/uv_os_setpriority through our C++ OS host and
expose getPriority(pid?) and setPriority(priority)/setPriority(pid, priority) in the
normal os builtin object and ESM shim. No new dependency, Node/V8, renderer service,
Ladybird seam or vendored change.

The JS wrapper validates signed 32-bit integer PIDs and integer priorities from -20
to 19, with pid validation before priority validation. Undefined getter PID means
current process. Undefined setter second argument selects its one-argument overload.
Type errors use ERR_INVALID_ARG_TYPE and numeric range errors ERR_OUT_OF_RANGE;
validation does not coerce non-numbers. The native boundary independently checks
finite integral ranges before converting to C++ int, protecting syscalls from
invalid internal arguments. Kernel/libuv errors use the existing SystemError
ERR_SYSTEM_ERROR/errno/syscall/info path. Setter success returns undefined.

Two intentional differences are measured against installed Node 24.15.0:

- Node's getPriority(-0) child aborts at a native IsInt32 assertion. Mite normalizes
  negative zero to zero at the native boundary; getter and setter PID -0 work.
  Mite priority -0 is accepted as numeric zero too.
- Node's invalid-argument error formatter reads properties of a hostile proxy and
  propagates its thrown Error. Mite emits its TypeError/code without accessing the
  argument object's properties. Exact error message/inspection behavior is not parity.

These are explicit differences, not exclusions hidden inside the shared corpus.

## Evidence

The mite-os-priority differential test compares 106 normal observations with Node:
types, integer/range boundaries, overload/undefined handling, validation precedence,
ESRCH/EINVAL getter error metadata, own PID/default getter, same-priority setters,
ignored receiver, function name/length and CJS/ESM named identity. A disposable child
then changes only its own nice value 0 -> 1 -> 19 and verifies both setter forms.
No foreign process priority is modified and the test parent keeps its priority.

Separate controls record the Node negative-zero abort and Mite's successful getter/
setter behavior, plus hostile-argument reflection versus Mite's zero-trap validation.
The reference control also accepts a future Node version that successfully normalizes
negative zero. Windows class mapping is accounted for in the fixture but not natively
validated on this host.

All 32 standalone CTest suites pass in build/os-priority-ctest.log. The detailed
priority differential output is in build/os-priority-differential.log. Firefly's
main-process ESM/HTTP fixture gets and confirms its own original priority without
changing it; the full existing Electron-reference gate passes in
build/os-priority-firefly-test.log. Existing OS/data/network/unsupported gates pass.

## Limits

OS constants are still explicitly unsupported. Other-process permissions, priority
boosting, native setter failures, privilege transitions, Windows class/elevation
mapping and Linux native behavior were not exercised. No complete error accessor/
constructor/stack formatting or monkey-patched-intrinsics compatibility is claimed.
The API exposes only the caller's existing OS permissions and is absent from the
renderer. Public preload/IPC, packaging, other runtime/platform milestones and a
full audit remain open. Existing Rust archive/deployment warnings persist and
Ladybird remains unmodified at the same pin.

Reference: official Node v24.15.0 [OS wrapper](https://github.com/nodejs/node/blob/v24.15.0/lib/os.js),
[argument validators](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/validators.js)
and [libuv priority API](https://docs.libuv.org/en/v1.x/misc.html#c.uv_os_setpriority).
These implementations are not embedded.
