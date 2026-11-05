# Security boundaries for application authors

This is guidance for the implemented development slices. Mite/Firefly have not had
a complete security audit or production-hardening sign-off. Public preload and
privileged IPC remain unsupported. Current support/evidence is in the
[Firefly matrix](../compatibility/firefly-0.1.md) and [process model](../concepts/runtime-and-processes.md).

## Place capabilities in the right process

Mite/main code can access its supported native files/network/OS APIs. Renderer
content runs LibWeb/LibJS and must not receive process, require, Buffer or native
module access. Firefly currently accepts only the restricted defaults:
contextIsolation true, nodeIntegration false and sandbox true. Unsafe values and
preload options explicitly reject.

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
such as permission handlers, navigation hooks, shell controls and public bridge APIs
must not be assumed present.

## Native services and local HTTP

Validate file paths, options, payload sizes and error behavior in main-process code.
A loopback listener is still reachable by other local processes. Serve only public
assets and keep privileged operations out of an unvalidated local route. The Designer
sample's production server is read-only and path-bounded; its smoke report exists
only in explicit test mode and does not grant native privileges.

## Bridge requirements still ahead

Before public IPC ships, it needs explicit channel/capability policy, bounded data
and pending work, sender/frame validation, lifetime/revocation, private error handling
and adversarial renderer tests. No working preload or contextBridge recipe is offered
until those gates pass. See the [baseline](../reference/documentation-baseline.md)
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
