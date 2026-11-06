# Troubleshooting

Start with the exact executable/commit, host and API mode. The [support policy](../reference/support-policy.md)
and compatibility matrices distinguish partial behavior from absent APIs.

## Common symptoms

| Symptom | Check / next action |
| --- | --- |
| `mite: unsupported option` / exit 9 | Use the [implemented CLI flags](../api/cli.md); Node/V8 flags are not accepted |
| Entry cannot be read | Verify cwd, filename, extension and native access; run from the repository root for its examples |
| ESM syntax in a .js file fails | Set package.json type=module or use .mjs; syntax auto-detection is absent |
| Module cannot be resolved | Inspect explicit relative extensions, package type/exports/imports and node_modules layout; reduce to one specifier |
| Named CommonJS import is missing | Consume its default/module.exports value; static named CJS detection is unsupported |
| A built-in property is undefined | Check the implemented export surface; whole-module Node parity is not promised |
| `fetch`, child_process or TLS call is absent | These are not implemented; check the roadmap rather than using a silent substitute |
| Console output is `[object Object]` | Node object inspection is absent; use an intentional JSON/string diagnostic |
| Process stays alive | Close servers/sockets/handles and inspect referenced timers/work; see async/resource lifetime |
| Program exits before an unref timer runs | An unreferenced timer alone does not keep the event loop alive |
| Nonzero exit after a Promise/callback failure | Handle the actual Promise/error callback; process exception/rejection event APIs are not implemented |
| `ERR_FIREFLY_NOT_READY` | Await app.whenReady before constructing BrowserWindow |
| `ERR_FIREFLY_UNSUPPORTED_API` | Remove the unsupported option/mode and review the reference; ES module preloads/unsafe web preferences intentionally reject |
| `ERR_FIREFLY_OBJECT_DESTROYED` | Stop using the window/webContents after destruction |
| `ERR_ABORTED` during loading | Check overlapping navigation, window closure or forced exit |
| `ERR_TIMED_OUT` during loading | Loading has a bounded deadline; check URL/resources/network and the load error |
| No `require` / `process` in renderer | This is the intended capability boundary, not a missing preload workaround |
| Build rejects modified upstream | Restore/review through the [pin update procedure](../development/updating-ladybird.md); do not bypass the guard |
| Windows checkout rejects a device path | Use the [tracked-path guard](../development/windows-build.md#checkout-guard); preserve a materializable source tree |
| Native window capture fails on locked macOS session | Rerun the visible gate unlocked; keep that acceptance item open |
| macOS dependency deployment-target / duplicate Rust symbols warn | Preserve the recorded limitation; do not infer older-host support from a successful local link |

## Package questions

npm remains an external package manager. A pure JS package may still need unsupported
native services or optional adapters. Check the [package corpus](../compatibility/npm-packages-0.1.md)
and compare a reduced fixture with Node. Native addons/V8 internals are outside the
initial required compatibility scope.

## Desktop and release questions

Changing electron to firefly works only for applications within the supported slice.
The [migration guide](../migration/from-electron.md) lists blockers. Experimental
macOS bundles are ad-hoc signed, not notarized; native Windows/Linux packages and a
complete cross-platform release are still unfinished. A generated draft workflow or
compiled web tree is not a usable package for an unverified platform.

## File a useful report

Include a minimal example, the exact command/commit, host/architecture, reference
version, stdout/stderr, error code and exit status. Avoid secrets/private payloads.
State whether the issue occurs in main code, renderer content, a native helper or
packaging. See [testing/debugging](testing-and-debugging.md) for reproductions and logs.
