Experimental Mite + Firefly build, including pinned Web Component Designer and
explicitly partial Code-OSS samples.

This is the template for a future four-product release, not an announcement of a
new successful package build or publication. Historical Alpha 1/2 retain their
original three-product archives and recorded source/version metadata.

These projects implement tested subsets of Node/Electron APIs on LibJS/LibWeb.
Read the included BUILD.json for exact revisions, architecture, build host and
signing status. The current pipeline produces four original product archives plus
SIZE and SHA256SUMS records. Each archive retains dependency notices.

Mite is independent of Firefly/LibWeb. Firefly apps use Ladybird renderer processes;
Node/V8/Electron are never bundled as a runtime fallback. General desktop/input,
API, extension and web-platform compatibility remain incomplete. Windows providers
and AppContainer/broker components do not establish a complete Windows executable
release; full Windows desktop/confinement and packaging remain blocked.

Code-OSS uses the exact pinned application with explicit local-desktop adaptations,
genuine Mite APIs and static builtin extensions only. Webviews, remote modes,
engine code cache, native-addon ABI compatibility and unbuilt runtime extensions
remain outside this package's accepted scope. Its physical metadata and file/link
inventory are sealed and rechecked during packaging. A release must pass both the
source-stage workflow and an actual relocated native Launcher CJS/editor/save,
terminal and parent-liveness gate; source tests alone certify neither.

macOS development packages are ad-hoc signed and not notarized. Only the native
host actually validated by the matching build is covered; older macOS versions
are not promised. Designer uses frameless HTML window controls and Electron-style
drag CSS. Its production payload bundles browser modules/workers and omits dev-only
npm dependencies; native packages strip nonessential symbols and report measured
sizes. Designer smoke coverage checks boot, one document edit, Monaco, window
controls and absent Node globals; it does not establish all designer,
collaboration, GPU/LLM or worker behavior.

See the [release guide](releases.md) and [Code-OSS package contract](../guides/code-oss-packaging.md)
for pending acceptance and the exact pipeline. No latest-SDK package or new release
is claimed until those required native jobs complete successfully.
