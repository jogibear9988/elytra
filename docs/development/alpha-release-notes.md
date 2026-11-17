Experimental **macOS ARM64 only** alpha template for Mite, Firefly, pinned Web
Component Designer and explicitly partial Code-OSS. Native Windows/Linux desktop
packages remain incomplete; this does not satisfy the full cross-platform target.

The current pipeline requires four product archives plus SIZE and SHA256SUMS.
This template is not a published release. Historical Alpha 1/2 keep their original
three-product/five-asset payloads, exact source revisions and snapshot filenames;
no historical bytes or metadata are rewritten by this change.

The samples use LibJS/LibWeb with explicit partial compatibility, production
payloads, preserved helper security metadata and notices. Code-OSS ships the pinned
source-derived development renderer and static builtin extensions, with recorded
local-desktop adaptations. It contains no Node/V8/Electron engine fallback. Native
addon ABI, webviews, remote modes, engine code cache, broader extension/input
compatibility and unbuilt runtime extensions remain unsupported or unaccepted.

Before publication, the matching source must pass required native package gates,
including source-stage Code-OSS CJS/editor/terminal/parent-liveness acceptance and
its actual relocated signed Launcher workflow. Neither source seals nor a local
CLI pass establish this acceptance. The latest SDK/four-product native pipeline is
still pending; no new release is announced here.

Apps are ad-hoc signed, **not notarized**, and conservatively target the build
host's macOS version. Review compatibility matrices, security guidance and exact
BUILD.json/source/run metadata before using an alpha. No production security audit,
older-host support or full Windows desktop implementation is claimed.
