Experimental **macOS ARM64 only** alpha of Mite, Firefly and the pinned Web Component
Designer demo. Native Windows/Linux desktop packages remain incomplete; this is not
the complete cross-platform release target.

Includes a LibJS-powered runtime, Ladybird renderer, frameless Designer HTML toolbar
with Electron-style drag regions and window controls, production npm dependencies,
stripped nonessential native symbols, preserved helper security metadata and notices.
SIZE-macos-arm64.json records actual uncompressed bundles and compressed downloads.

Apps are ad-hoc signed, **not notarized**, and target the build host's macOS version.
Public preload/contextBridge/IPC and broad Node/Electron compatibility are incomplete.
Review compatibility matrices, security guidance and the source/build metadata before
using an alpha. No production security audit or older-host support is claimed.
