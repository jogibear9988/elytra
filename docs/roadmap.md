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
At 5735b91 native Linux/Windows Mite pass 74/73 suites. Firefly has scoped X11
app/screen/clipboard, native alias, portable build and archive/Designer acceptance.
New changes still require their own native gates. The published macOS Alpha 2 and later optimized Actions artifact are
separate deliverables. A Windows desktop/package remains the largest platform gap.

## Implementation sequence

| Priority | Deliverable | Evidence needed before declaring it usable |
| --- | --- | --- |
| First compatibility target | Code-OSS desktop on Firefly/Mite | Follow the [pinned VS Code blocker audit](compatibility/vscode.md): real bootstrap (next observed blocker with explicit engine overlay: app.enableSandbox), binary/event/port IPC, workbench, workspace files, extension/utility processes and native service dependencies; no Electron/Node/V8 substitute |
| Current observed startup blocker | Real sandbox policy and demanded main services | Implement required native behavior, rerun the actual prepared main entry and record its next failure. Partial [application menus](api/menu.md) now pass local Qt/LibJS and public-host gates; the explicit [engine overlay](architecture/0131-code-oss-engine-defaults.md) separates implicit Chromium tuning, rejects incompatible user requests and preserves the sandbox branch. The real prepared entry now fails at app.enableSandbox (exit 1). Do not add inert exports to skip imports |
| Required renderer startup work | Actual custom resource protocols | The partial [protocol API](api/protocol.md) now publishes the native origin/trust table. Internal production-helper ESM/CSS/file loading passes locally. Default-session public file callbacks and automatic grants now pass local real-window acceptance. General Response handlers, partitions and additional privilege flags remain required. Local native crashReporter capture/recovery now passes; without the engine overlay the entry rejects Chromium enable-features; the opt-in overlay advances to missing app.enableSandbox. Preserve origin, CORS/CSP and lifecycle boundaries |
| Now, driven by Code-OSS | Secure public preload/bridge/IPC and efficient files | Build on validated [invoke/handle IPC](api/ipc.md); prioritize binary values, event IPC, MessagePorts and bounded [file transport](guides/file-transfer.md), retaining sender/revocation/adversarial gates |
| Required platform work | Native Windows bring-up and requested Designer release | Actual native compiler/runtime results, Firefly window/process/sandbox tests, Designer acceptance, relocatable Windows package and successful required release jobs |
| Next, chosen from Code-OSS blockers | Required Node services for practical apps | Bootstrap imports, filesystem/watch/stream/process behavior and auxiliary hosts first; implement verified compatible native services and document engine-specific adaptation |
| Next, chosen from Code-OSS blockers | Practical Electron migration services | Required preload/window/protocol/session/dialog/menu services first, with Electron migration fixtures; broader example coverage follows |
| Next | Native Linux and platform-complete CI | Native builds/tests, renderer and process/security evidence, packages and launch acceptance on every required OS |
| Next | Real application compatibility | Broader locked npm packages, unmodified application entry points where possible, meaningful Designer feature coverage and recorded differences |
| Later, after current platform work | HTTP/2 and HTTP/3 in Mite's HTTP stack | Native protocol client/server slices, secure transport, independent interoperability and lifecycle/security tests; scope below |
| Before production claims | Distribution and operational maturity | Notices, signing/notarization, installer/update policy, release/upgrade tests, reproducibility and security review |

Upstream remains pinned and unmodified. Changes belong in our adapters/build files;
no Node, V8, Chromium or WSL runtime fallback is permitted by the specification.

Priority updated 2026-11-07 at the user's request: general compatibility work should
advance actual Code-OSS startup and developer workflows first. This does not remove
native Windows/Linux, Designer, security, packaging or documentation requirements.
The [Code-OSS milestones](compatibility/vscode.md#implementation-order) distinguish
a running workbench from full workspace, extension and terminal acceptance.

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
