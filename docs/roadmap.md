# Delivery and documentation roadmap

This roadmap preserves the full product specification. Completed vertical slices
are recorded in [PROGRESS](PROGRESS.md); this page describes remaining deliverables.
A documented plan is not an implemented or tested API. No release dates are promised.

## Current measured foundation

Mite has a real LibJS CLI and selected Node-style modules, native I/O, timers,
streams/networking and crypto foundations on tested macOS ARM64 and a scoped
[Linux x64 runtime baseline](development/linux-build.md). Firefly has a native
macOS window/lifecycle/loading slice, isolated renderer defaults and a runnable pinned
Designer sample. Experimental relocated macOS bundles pass focused tests. One-way/synchronous/port IPC, full preload/contextBridge compatibility, broad Node/Electron parity and complete native
Windows/Linux releases remain unfinished.

The [current support page](status.md) records exact tested sources and examples.
Native Linux/Windows Mite now pass 65 suites. Firefly has scoped X11 app/clipboard
acceptance; new screen wiring, portable builds and archive/Designer gates remain
pending. The published macOS Alpha 2 and later optimized Actions artifact are
separate deliverables. A Windows desktop/package remains the largest platform gap.

## Implementation sequence

| Priority | Deliverable | Evidence needed before declaring it usable |
| --- | --- | --- |
| Now | Native Windows bring-up and requested Designer release | Actual native compiler/runtime results, Firefly window/process/sandbox tests, Designer acceptance, relocatable Windows package and successful required release jobs |
| Now | Secure public preload/bridge/IPC | Build on validated [invoke/handle IPC](api/ipc.md) and isolated preload publication; extend message modes, clone types and platform coverage while retaining sender/revocation/adversarial gates |
| Next | Required Node services for practical apps | Missing globals/built-ins and options, including network clients/TLS/fetch, child processes and crypto services, verified against Node |
| Next | Practical Electron migration services | Native dialog/menu/tray/clipboard/shell/screen/image/session slices with Electron migration fixtures |
| Next | Native Linux and platform-complete CI | Native builds/tests, renderer and process/security evidence, packages and launch acceptance on every required OS |
| Next | Real application compatibility | Broader locked npm packages, unmodified application entry points where possible, meaningful Designer feature coverage and recorded differences |
| Later, after current platform work | HTTP/2 and HTTP/3 in Mite's HTTP stack | Native protocol client/server slices, secure transport, independent interoperability and lifecycle/security tests; scope below |
| Before production claims | Distribution and operational maturity | Notices, signing/notarization, installer/update policy, release/upgrade tests, reproducibility and security review |

Upstream remains pinned and unmodified. Changes belong in our adapters/build files;
no Node, V8, Chromium or WSL runtime fallback is permitted by the specification.

## Planned HTTP/2 and HTTP/3

Requested 2026-11-07. This is future implementation work, below current Windows/
Linux desktop, Designer and distribution priorities. Today's HTTP/HTTPS/Fetch
implementation remains HTTP/1; neither HTTP/2 nor HTTP/3 is supported yet.

- **HTTP/2:** add a Node-compatible node:http2 client/server surface in vertical
  slices, including sessions/streams, multiplexing, headers/trailers, flow control,
  cancellation, stream/session errors and graceful shutdown. Use verified TLS/ALPN
  for secure HTTP/2 and explicitly document any unsupported cleartext mode.
- **HTTP/3:** add a native QUIC/TLS transport and HTTP/3 client/server support.
  Select a maintained, pinned implementation with preserved notices. Choose the
  public API against the then-current Node/standards baseline; do not invent a
  Node-compatible node:http3 promise before such a baseline is verified. Document
  UDP, certificate, connection lifecycle and platform requirements explicitly.
- **Shared HTTP consumers:** design the native transport boundary so Mite Fetch
  and eligible HTTP clients can reuse the new protocols with documented negotiation,
  pooling and fallback rules. Preserve current HTTP/1 behavior and public contracts;
  the node:http/node:https API must not silently become node:http2. Mite remains
  independent of Firefly/LibWeb, Node and V8. Renderer networking is a separate
  upstream integration concern.
- **Acceptance:** require real independent HTTP/2 and HTTP/3 peers on native
  macOS/Linux/Windows, protocol/version evidence, concurrent streams, pressure,
  cancellation, malformed inputs, bounded resources, TLS verification/refusal and
  shutdown. Compare HTTP/2 behavior against Node where applicable. Mark individual
  APIs/options partial until tested; unsupported modes must fail explicitly.

Choose dependencies and record an ADR when this work starts. Add detailed API
references and compatibility entries alongside each working slice; this roadmap
entry does not change current support status.

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
