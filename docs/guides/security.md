# Security boundaries for application authors

This is guidance for the implemented development slices. Mite/Firefly have not had
a complete security audit or production-hardening sign-off. Bounded public
[preload/contextBridge](../api/context-bridge.md) and bounded
[invoke/handle IPC](../api/ipc.md) are available. Current support/evidence is in the
[Firefly matrix](../compatibility/firefly-0.1.md) and [process model](../concepts/runtime-and-processes.md).

## Place capabilities in the right process

Mite/main code can access its supported native files/network/OS APIs. Renderer
content runs LibWeb/LibJS and must not receive process, require, Buffer or native
module access. Firefly currently accepts only the restricted defaults:
contextIsolation true, nodeIntegration false and sandbox true. Unsafe and
unknown preference values explicitly reject. A selected preload publishes to every
supported top-level document loaded into that window; no origin allowlist exists.

Do not work around missing bridge support by exposing a native loader to document
code or borrowing an internal test endpoint. Internal realm/callback/snapshot tests
are implementation evidence for selected boundaries, not a complete public API.

## Treat application content and data carefully

Use trusted local application assets for the current first-app path. Review browser
code, dependency changes and any external resources the application loads. Use an
appropriate Content Security Policy, avoid evaluating untrusted strings, and assign
untrusted text through textContent rather than treating it as markup. These choices
do not establish universal Ladybird/Chromium web-platform parity.

Electron's [security guidance](https://www.electronjs.org/docs/latest/tutorial/security)
is a useful application-review baseline: minimize renderer privileges, validate
communication/navigation decisions and keep shipped dependencies current. Firefly
cannot yet implement all of Electron's permission/session/navigation controls. Methods
such as permission handlers, navigation hooks, shell controls and additional bridge APIs
must not be assumed present.

## Native services and local HTTP

Validate file paths, options, payload sizes and error behavior in main-process code.
A loopback listener is still reachable by other local processes. Serve only public
assets and keep privileged operations out of an unvalidated local route. The Designer
sample's production server is read-only and path-bounded; its smoke report exists
only in explicit test mode and does not grant native privileges.

## Request/reply authorization

Expose fixed actions that call fixed IPC channels. Each main handler must check the
native event.sender and senderFrame URL before performing privileged work. Caller
arguments never establish sender identity. The [IPC reference](../api/ipc.md)
documents data/pending limits, sanitized errors and document/window revocation.
Native tests exercise those boundaries; broader event/port/subframe IPC and a
complete security audit remain outstanding. See the [baseline](../reference/documentation-baseline.md)
and [roadmap](../roadmap.md).

## Dependency and distribution review

Keep the Ladybird source pin/gitlink synchronized and unmodified; use the documented
update procedure and rerun native gates. Preserve source/native/npm/Rust notices.
Do not remove helper sandboxing or entitlements to obtain a successful package run.
Ad-hoc signing and a successful screenshot are not notarization or security review.
Use the [release guide](packaging.md) to inspect actual artifacts and remaining gates.

## Reporting a suspected issue

Reduce the issue to the smallest reproduction and identify the exact commit,
platform, main/renderer context and expected boundary. Include sanitized diagnostics;
avoid publishing credentials, private files or a live exploit against other users.
A dedicated private disclosure channel/policy is still a project governance item;
do not claim that Node/Electron disclosure channels cover this project.

## Designer window controls

The frameless Designer has a sample-local, fixed window capability for minimize,
maximize/unmaximize, restore, state and close. Requests require a random per-launch
token, exact same-origin/Host checks and POST with no body. It does not accept script,
filesystem paths or arbitrary IPC channels. Static assets remain publicly readable
on loopback while the app runs, so this does not authenticate against other local
processes. The capability grants only this app window's cosmetic/lifecycle controls;
it is separate from the bounded public request/reply IPC bridge.
