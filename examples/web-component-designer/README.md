# Web Component Designer on Firefly

This sample builds the real [web-component-designer-demo](https://github.com/node-projects/web-component-designer-demo)
at commit `0b3c56bcd30c8047b14f847c7ec1ffab114cc597`. The source revision and
package-lock hash are pinned in upstream.json; npm ci uses that lockfile. The upstream
MIT license and dependency package/license files are retained in the staged output.
Ladybird is unmodified.

## Build and launch (tested macOS ARM64)

First build Firefly as described in docs/development/renderer-build.md, then:

```sh
python3 tooling/build_designer.py
build/renderer/bin/firefly build/designer-app/main.mjs
```

The first command downloads the pinned source, installs with implicit lifecycle
scripts disabled, explicitly runs its reviewed Monaco worker/editor build and then
compiles TypeScript. Our build then bundles/minifies the browser module graph into
shared esbuild chunks and minifies Monaco workers. The pinned lockfile supplies the
esbuild build tool. CSS modules and original asset URL bases are adapted explicitly;
classic import-map/touch bootstraps remain available. App/package notices are kept
under licenses, outside the served web root.
Node/npm are build tools, not bundled renderer or main-process engines.

Use a fresh --output directory for a second staging run. Source checkouts with local
changes or a different origin reject. The upstream Monaco preparation modifies its
installed node_modules, not the pinned Git source. Upstream's own package lock is
also included in the staged application for provenance.

The Mite main process serves only the compiled public web tree on an ephemeral
127.0.0.1 port. Firefly loads that origin in an ordinary isolated BrowserWindow.
Loopback HTTP retains the demo's module/import-map/worker URL layout without rewriting
its source or adding filesystem privileges to the page. Static assets accept GET/HEAD; paths cannot escape the staged web directory.
Six fixed window-control actions use a separate app-local POST route with a random
per-launch token plus exact Origin/Host validation. It accepts no body, arbitrary
channel, script or path. This is not public Firefly preload/IPC. The web assets contain
no application secrets and can be read by other local processes while the app runs.

## Custom HTML window chrome

The sample uses BrowserWindow({frame:false}) and its existing HTML toolbar. The
window-controls.css overlay uses the same app-region/-webkit-app-region drag and
no-drag declarations as Electron. Existing toolbar buttons remain clickable; added
buttons minimize, maximize/restore and close the native window. Double-click on a
drag region toggles maximize. The pinned upstream checkout stays untouched; only
our staged HTML receives the controls overlay.

Window actions call the corresponding Electron-style BrowserWindow methods in the
main process through the sample's fixed window capability. The renderer receives no
Node/filesystem globals. See [custom chrome limits](../../docs/api/browser-window.md#custom-title-bar-and-dragging).

## Dependency and bundling modes

Default: --dependencies production --bundle. Packages marked exclusively dev in the
pinned npm lock are omitted; shared/optional runtime packages remain eligible. npm
installation still includes build tools before compilation. Bundling removes the
source module/build payload, retains required browser chunks/workers/assets, and
moves npm notices/package metadata to licenses/npm. The DXF browser entry is resolved
from the pinned package's actual module field because upstream's HTML names an
absent index.esm.js file.

```sh
python3 tooling/build_designer.py --output build/designer-production --dependencies production
python3 tooling/build_designer.py --output build/designer-modules --dependencies all --no-bundle
```

The second mode retains installed modules for development and debugging. Bundling
is validated for the recorded app/edit/Monaco/control workflow, not every optional
export/collaboration/GPU/LLM mode or arbitrary dynamically requested source module.
Keep module mode for investigating features outside that measured graph. No generic
npm compatibility or complete worker-language acceptance is implied by bundle size.

## macOS app output

build_designer.py stages JavaScript/assets; it does not itself emit a Finder app.
The packager consumes that staging directory and native Release builds:

```sh
python3 tooling/package_macos.py --mite-build build/release-mite \
  --renderer-build build/renderer --designer build/designer-production \
  --output build/designer-release --version 0.1.0-alpha.1 \
  --dependencies production --symbols stripped
```

The app is build/designer-release/Web Component Designer.app. The native release
CI driver performs both steps automatically and archives the result. SIZE.json
reports actual uncompressed framework/sample bytes; archive size is separate.
Symbols mode keep retains local/debug symbols; stripped keeps dynamic exports and
re-signs while preserving helper entitlements/flags. Packages are ad-hoc signed,
not notarized. [Native release documentation](../../docs/development/releases.md)
explains the remaining platform/release gates.

## Native acceptance

```sh
python3 tooling/tests/test_designer.py --mite build/debug/bin/mite \
  --app build/designer-app --firefly build/renderer/bin/firefly
```

The test checks asset/worker MIME, HEAD, malformed/traversal paths, missing files and
read-only serving. The real Firefly --smoke launch then waits for the demo's document
container, sets and commits a button, checks content round-trip and the Monaco editor,
checks absent process/require globals, exercises actual HTML minimize/maximize/restore
buttons, checks denied capabilities, reports success and closes the native window.
In smoke mode only, the main process observes the native minimize event and restores
the window so the test never depends on a paused renderer restoring itself. Fixed
startup-stage diagnostics and stdout/stderr are retained in build/designer-last-ui.log.
The tokenized, bounded smoke report route exists only in explicit --smoke mode. It
is a sample-local test, not a public Firefly privileged IPC bridge or a security audit.

Locally the full test passes on macOS ARM64. A separate actual LibWeb compositor
capture was inspected at build/designer-mite-ladybird.png. Its toolbar, palette,
docking layout, canvas and Monaco stylesheet editor render. Native desktop pixel
presentation on the locked host still needs an unlocked-session review. All designer
commands, drag/drop, worker modes, collaboration/WebRTC, GPU/LLM, downloads, external
npm loading and offline behavior are not yet verified. Some optional upstream demo
features contact external services. Browser/engine capabilities determine support.

## Releases and Windows

The same pinned compiled web payload is intended for macOS and native Windows apps.
The native Windows Firefly renderer/window/process/sandbox adapters and packaging
are not implemented/validated yet. This sample does not substitute Electron, Node,
V8, Chromium, WebView2 or WSL when Firefly cannot run. Release build automation must
fail before publishing a complete cross-platform release if a required platform is
missing. macOS bundle/relocation and release workflow work are tracked in PROGRESS.
