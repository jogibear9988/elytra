# ADR 0144: Restricted native renderer process metadata

Status: accepted for the tested local macOS Release subset.

## Context

The pinned Code-OSS window supplies additionalArguments containing its configuration
resource. Its isolated preload reads process.argv, invokes a configuration channel,
merges userEnv, then publishes selected process getters through contextBridge.
Getter publication alone did not supply the underlying renderer process data.
Mite's privileged process object cannot be installed in an untrusted renderer.

## Decision

Use small C++ adapters at the existing native window/preload boundary. Validate
application arguments as exact native realm-owned arrays with own string entries,
strict Unicode/NUL/count/byte limits, and copy them into the per-window preload
configuration. Snapshot the main OS environment through libuv at construction.
Share a pure AK versioned metadata decoder between main and helper; the helper
independently validates collections, string types, schema and aggregate budgets.
No Mite/libuv dependency enters the renderer process builder.

Cache the actual helper executable before upstream bootstrap/sandbox. Immediately
before evaluating each product preload, construct a fresh ordinary source-realm
process object with actual OS PID, platform/architecture, executable and truthful
framework/pinned-engine versions. Source argv/env containers are mutable local
copies; reload restores the window snapshot. Define own properties without magic
__proto__ assignment. Do not install this object into principal/child-frame or
SDK/diagnostic worlds. Retain the native enforced sandbox and isolated-world policy.

The logical argv is execPath followed by app arguments, with exact order, empty
strings and duplicates. Do not inject untrusted app strings into upstream helper
OS options. This deliberately differs from Electron's internal Chromium argv prefix
and its measured macOS switch/positional reordering and empty argument omission.
Do not fabricate Node/V8/Electron identities. Metrics and process events are not
implemented: demanded on/getProcessMemoryInfo names throw an explicit unsupported
error and their native functions are blocked from bridge publication.

Keep source limits separate from process metadata limits: 64 arguments / 4 KiB each /
16 KiB combined; 1,024 environment entries; 64 KiB combined argument/environment
content per window; 1 MiB source and 1 MiB process content across at most 16 configured
windows. Require a configured preload for nonempty additionalArguments.

## Acceptance

Native schema tests exercise roundtrip, primitive/schema/version/type refusals,
__proto__/Unicode data, NUL key/value/argument rejection and exact/overflow argument,
environment and byte budgets. Complete native isolation tests pass. The first
production-helper test found an unpaired-surrogate gap in AK's default conversion;
explicit AllowLonelySurrogates::No now rejects that case.

A focused hidden two-window/reload gate checks original argv array mutation,
per-window initial environment, source-local changes/userEnv merge, early config IPC,
real PID/executable/platform/architecture/version data, unsupported errors, native
function export refusal and principal/child-frame isolation. Python independently
observes live helper OS parent identity. Actual Electron 45.2.3 passes the common
subset; exact argv prefix/order/empty strings, framework identity and unsupported
behavior are checked separately as Firefly contracts. The existing full public
preload/reference and actual kernel sandbox regression gates pass locally.

The real prepared Code-OSS entry still exits 1 at allowServiceWorkers using an
existing temporary user-data directory. Full preload process events/metrics, zoom,
file APIs, ports, worker/cache/resource routing and a real workbench open/edit/save
milestone remain outstanding. No fresh Linux/native Windows acceptance is claimed.
Ladybird stays unmodified and Mite remains independent. The full goal stays active.
