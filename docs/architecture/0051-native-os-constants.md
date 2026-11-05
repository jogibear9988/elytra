# 0051: Export OS constants from native headers

Status: node:os constant groups tested on macOS ARM64. Native Windows/Linux values,
optional SDK symbols and complete OS-module behavior remain unverified.

## Decision

Add an owned C++ OSConstants adapter using errno/signal/dlopen headers and the
existing libuv header. Public Node-compatible identifier lists are conditional on
native symbol availability; numeric values come from the selected host SDK/libuv.
No runtime Node/V8, build-time Node query, downloaded constant snapshot, new library
or Ladybird source seam is introduced. The Node 24.15 API is the reference surface,
not a dependency embedded in Mite.

Create null-prototype objects for constants, dlopen, errno, signals and priority.
Individual numeric properties are enumerable, nonwritable and nonconfigurable.
Group properties remain writable/configurable and groups remain extensible, matching
the reference, except signals is frozen natively through LibJS before publication.
Native freezing remains effective when application code replaces Object.freeze
before importing os. The os.constants property itself is enumerable/read-only/
nonconfigurable; the same object is published by the CJS default and ESM named
export. No group or numeric data is reconstructed per read.

Native priority aliases use UV_PRIORITY_* rather than hardcoded host numbers.
UV_UDP_REUSEADDR and loader flags are data exports; they do not implement UDP
sockets, native addons/dlopen or signal event handling. OS support for those APIs
remains governed by the other compatibility rows. Missing optional native constants
are absent, as in the reference's conditional API, not fabricated zero values.

## Evidence

The mite-os-constants differential gate compares all 121 available numeric values
on this macOS host with installed Node 24.15.0, including all names/order, numeric
types, descriptors, null prototypes, extensibility/freeze state and module identity.
Strict and Reflect mutation/deletion controls verify immutable numbers and the
export property, while mutable group replacement/extensions preserve shared ESM
identity. A separate child replaces public Object.freeze before import; the native
signal group remains frozen in both runtimes. Default/namespace export-name sets
also match the Node surface; this is not full function metadata parity.

All 33 standalone CTest suites pass in build/os-constants-ctest.log. Firefly's
main-process ESM/HTTP migration fixture imports constants, checks native errno/
priority data and signal freezing, then retains renderer isolation. Its existing
full Electron-reference gate passes in build/os-constants-firefly-test.log. The
standalone binary still links platform libraries only, without Node/V8/LibWeb/Qt.
The same runtime source graph builds both hosts; vendor integrity stays clean.

## Limits

This completes the currently targeted os export-name surface on the tested host,
not full Node compatibility or another checked project milestone. Platform-specific
constant sets/values need native Windows/Linux runners. The previous OS error,
priority negative-zero/formatter differences, privilege/environment limitations,
real unusual data and reconfiguration/error paths remain documented in ADRs
0047–0050. No broad monkey-patched-intrinsics, new signal/UDP/addon implementation,
new npm/platform/packaging, full audit or visible-desktop claim. Public preload/IPC
and the remaining runtime/renderer milestones stay open. Existing build warnings
persist; Ladybird's pin/source remain unchanged.

Reference: official Node v24.15.0 [constant API definitions](https://github.com/nodejs/node/blob/v24.15.0/src/node_constants.cc)
and [OS export descriptors](https://github.com/nodejs/node/blob/v24.15.0/lib/os.js).
Our adapter uses native symbols; it includes no Node implementation files.
