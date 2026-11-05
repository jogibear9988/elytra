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
compiles TypeScript. It stages HTML/assets/dist/node_modules plus our main process.
Node/npm are build tools, not bundled renderer or main-process engines.

Use a fresh --output directory for a second staging run. Source checkouts with local
changes or a different origin reject. The upstream Monaco preparation modifies its
installed node_modules, not the pinned Git source. Upstream's own package lock is
also included in the staged application for provenance.

The Mite main process serves only the compiled public web tree on an ephemeral
127.0.0.1 port. Firefly loads that origin in an ordinary isolated BrowserWindow.
Loopback HTTP retains the demo's module/import-map/worker URL layout without rewriting
its source or adding filesystem privileges to the page. Only GET/HEAD are available
in normal mode; paths cannot escape the staged web directory. The web assets contain
no application secrets and can be read by other local processes while the app runs.

## Native acceptance

```sh
python3 tooling/test_designer.py --mite build/debug/bin/mite \
  --app build/designer-app --firefly build/renderer/bin/firefly
```

The test checks asset/worker MIME, HEAD, malformed/traversal paths, missing files and
read-only serving. The real Firefly --smoke launch then waits for the demo's document
container, sets and commits a button, checks content round-trip and the Monaco editor,
checks absent process/require globals, reports success and closes the native window.
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
