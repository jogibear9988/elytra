# 0068: Application-selected CommonJS preloads

Status: implemented and locally tested on macOS ARM64; IPC and other desktop
platforms remain incomplete.

## Decision

Compose the existing isolated-world, job-ownership and context-publication engines
into an application-facing BrowserWindow.webPreferences.preload path. Use a separate
production WebContent helper with no compiled diagnostic APIs. Mite still depends
only on standalone LibJS in its independent build; no Node/V8 fallback is added.

The main process validates and snapshots an absolute regular UTF-8 CommonJS file
at window construction. Bound source/metadata by 64 KiB/4096 bytes, 16 configured
windows and 1 MiB total source. Retain a source snapshot across navigation, bind it
to the native view handle and send bounded source/metadata through the existing
internal session-state IPC. No document-controlled identifier selects a preload.
Do not expose a WebDriver endpoint or forward application preload entries to the
upstream principal-world preload evaluator.

Install source in top-level file/http/https document realms before author scripts.
Skip initial about:blank and child frames. Every document loaded into the window
receives the API; there is no origin allowlist. Same-origin children can access a
parent's published API normally. IPC and navigation/permission policy remain work.

Qt's upstream WebContentView is final. Generate one owned translation unit from
its pinned source, adding only our include and one call immediately after
ViewImplementation::prepare_page_for_navigable. Source/header hashes and exact
seam count gate both configure and build. Preserve copyright and every other byte;
never edit the vendored tree. The hook configures newly hosting pages before their
first script, including file-to-HTTP process replacement. Existing pages are also
configured before explicit host load requests. Process-wide configuration lists
all configured live native window handles, allowing shared renderer clients.

The production helper validates that narrow configuration, intercepts internal
realm lifecycle notifications and creates protected IsolatedWorld/ContextBridge
owners. Copy native source metadata before entering JS, retain exact document
identity, sweep inactive worlds and revoke on lifecycle/shutdown. Limit active
worlds to 32. Preload require grants only the contextBridge export from firefly;
CommonJS module/exports/filename/dirname are private wrapper parameters, not page
globals. Unsupported modules throw explicitly. The public API does not grant
main-process filesystem/OS/network capabilities.

Syntax or uncaught evaluation failure terminates the helper and rejects pending
loads via existing crash handling. This fail-closed policy differs from Electron's
preload-error/continue semantics and can affect cohosted documents. Diagnostics
omit private exception details. Implementing better error delivery is separate
work; do not silently continue a document missing its required initialization.

## Packaging and validation

Firefly selects FireflyRuntimeHelper in development. Packaging requires that
production helper and copies physical binaries into the app; it cannot fall back
to the diagnostic or upstream WebContent. Keep sandbox flags/entitlements, resource
staging, relocatable libraries, stripping/signatures and notices. The hello-world
sample includes a real preload and the packager carries it. Release builds run the
public preload suite against the packaged Firefly executable as well as native
Designer acceptance.

The shared real Electron/Firefly fixture tests first-script visibility, frozen
nested APIs and Promise results, three independent windows, no child installation,
fresh navigation and file-to-HTTP process replacement. Firefly-specific tests cover
invalid paths/files/types/UTF-8, 64 KiB boundary, unsupported require, CommonJS
metadata/receiver, source snapshot reuse, trap-free getter rejection and fail-closed
syntax/throw behavior. Existing SDK/helper gates retain the adversarial callback,
publication rollback, principal-reentry and detach/Promise revocation coverage.
These are scoped evidence, not a complete security audit, universal browser callback
support or native Windows/Linux desktop certification. Public ipcMain/ipcRenderer,
ESM preloads, general module resolution and Electron's broader bridge surface remain
unsupported. See the [API contract](../api/context-bridge.md).
