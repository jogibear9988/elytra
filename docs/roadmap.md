# Delivery and documentation roadmap

This roadmap preserves the full product specification. Completed vertical slices
are recorded in [PROGRESS](PROGRESS.md); this page describes remaining deliverables.
A documented plan is not an implemented or tested API. No release dates are promised.

## Current measured foundation

Mite has a real LibJS CLI and selected Node-style modules, native I/O, timers,
streams/networking and crypto foundations on tested macOS ARM64 and a scoped
[Linux x64 runtime baseline](development/linux-build.md). Firefly has a native
macOS window/lifecycle/loading slice, isolated renderer defaults and a runnable pinned
Designer sample. Experimental relocated macOS bundles pass focused tests. The public
preload/contextBridge/IPC APIs, broad Node/Electron parity and complete native
Windows/Linux releases remain unfinished.

## Implementation sequence

| Priority | Deliverable | Evidence needed before declaring it usable |
| --- | --- | --- |
| Now | Native Windows bring-up and requested Designer release | Actual native compiler/runtime results, Firefly window/process/sandbox tests, Designer acceptance, relocatable Windows package and successful required release jobs |
| Now | Secure public preload/bridge/IPC | Complete remaining realm/callback boundaries, validated bounded transport, sender/frame identity, capability revocation and malicious renderer fixtures |
| Next | Required Node services for practical apps | Missing globals/built-ins and options, including network clients/TLS/fetch, child processes and crypto services, verified against Node |
| Next | Practical Electron migration services | Native dialog/menu/tray/clipboard/shell/screen/image/session slices with Electron migration fixtures |
| Next | Native Linux and platform-complete CI | Native builds/tests, renderer and process/security evidence, packages and launch acceptance on every required OS |
| Next | Real application compatibility | Broader locked npm packages, unmodified application entry points where possible, meaningful Designer feature coverage and recorded differences |
| Before production claims | Distribution and operational maturity | Notices, signing/notarization, installer/update policy, release/upgrade tests, reproducibility and security review |

Upstream remains pinned and unmodified. Changes belong in our adapters/build files;
no Node, V8, Chromium or WSL runtime fallback is permitted by the specification.

## Documentation sequence

The [Node/Electron baseline](reference/documentation-baseline.md) defines the expected
coverage. Maintain these workstreams alongside implementation:

| Workstream | Present documentation | Remaining depth |
| --- | --- | --- |
| Start and learn | Documentation home, source quickstart, concepts and task guides | More verified recipes as missing APIs become available; end-user installation guides only after packages pass |
| Mite reference | API overview, selected detailed entries and compatibility matrix | Dedicated pages for every implemented module/class and supported overload; event/error/ownership tables |
| Firefly reference | Overview and app/window/webContents entries | Full implemented event/option coverage, then validated public bridge and desktop service entries |
| Migration | Node/Electron audits and current blockers | More real app/package reports with exact versions, changes and measured outcomes |
| Security and operations | Current boundary guidance, troubleshooting, release/build guides | Complete public IPC threat model, permissions/navigation/session policies, diagnostics and update/recovery guidance |
| Versions | Reference pins, ADRs and compatibility states | Actual release notes, API change records and deprecation/migration policy as releases exist |

Documentation work must not make implementation checkboxes green. Proposed APIs may
be discussed as requirements; unsupported calls do not belong in runnable quickstarts.

## Updating the roadmap

For a coherent implementation commit, update its reference, relevant example/tests,
compatibility state and PROGRESS together. For documentation-only commits, record
what was researched, written, linked and executed; leave platform/API acceptance
claims unchanged unless new evidence was obtained.
