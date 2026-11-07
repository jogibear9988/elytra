# ADR 0132: Require the existing native sandbox through app.enableSandbox

Status: accepted for the implemented main/BrowserWindow subset.

## Context

The actual Code-OSS entry with the explicit engine overlay reaches its unchanged
app.enableSandbox call. Firefly already launches renderers with Ladybird's OS
sandbox and rejects weaker web preferences. The public call must enforce a real
process policy, not merely resolve that missing method.

## Decision

Add a main-owned native policy latch with early/idempotent enable, native readiness
sealing and stopped-state rejection. Enable verifies the actual BrowserOptions
sandbox launch setting. Independently check that setting and native lifecycle at
the trusted window-creation entry before preload I/O or window construction. A
native disabled setting is an error both with and without the explicit global latch;
Firefly gains no unsandboxed renderer mode.

Expose app.enableSandbox on the existing main singleton/aliases. Set the JS global
requirement only after native success. When required, boolean sandbox:false in
BrowserWindow preferences is overridden without modifying the caller's object.
Malformed values, nodeIntegration:true and contextIsolation:false retain explicit
unsupported errors. No renderer/preload receives the native policy or app export.
Do not claim full Electron preference override semantics or sandbox the main process.

Keep Ladybird's sandbox installation/profile/bootstrap unchanged. Existing default
and pre-existing helper launch policy is already sandbox enabled. No private macOS
API enters product code, and standalone Mite remains independent of graphics.

## Acceptance and limits

Native tests independently exercise disabled launch refusal, early/repeated/late
calls, window-phase checks, attempted launch-policy mismatch and teardown. Node/Mite
policy checks cover failed-enable rollback, actual factory preference handling,
immutability of caller preferences and existing capability restrictions.

One public hidden-window fixture verifies that sandbox:false succeeds only under
early global enforcement, late enable fails, the real restricted preload publishes
a frozen bridge, and the page has no process/require/native bridge. The external
test checks exact owned PID ancestry and native launch flags, then independently
queries actual OS state. On macOS, sandbox_check(pid, NULL, 0) returns 0 for the
owned main and 1 for its renderer; this test-only null-operation ABI is documented
in [Chromium's Seatbelt implementation](https://chromium.googlesource.com/chromium/src/+/master/sandbox/mac/seatbelt.cc).
On Linux, acceptance requires NoNewPrivs, seccomp filter mode and more renderer
filters than the owned main, not page-supplied flags. Fresh hosted Linux execution
remains required. Windows desktop is unavailable.

These checks prove policy routing and sandbox activation, not an exhaustive OS
permission/escape audit, Chromium sandbox equivalence or all future window types.
No Node integration is added and no process.sandboxed property is fabricated.
The real prepared Code-OSS entry now passes this call and fails on unsupported
app.setPath('userData', ...), retaining exit 1. Workbench acceptance remains open.
See the [app contract](../api/app.md#appenablesandbox).
