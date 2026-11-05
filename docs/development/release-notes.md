Experimental Mite + Firefly build, including the pinned Web Component Designer demo.

These projects implement a tested subset of Node/Electron APIs on LibJS/LibWeb.
Read the included BUILD.json for exact revisions, architecture, build host and signing
status. Each archive contains dependency notices; checksums accompany the assets.

Mite is independent of Firefly/LibWeb. Firefly apps run Ladybird renderer processes;
Node/V8/Electron are never bundled as a fallback. Public preload/contextBridge/IPC,
complete API compatibility and broad web-platform compatibility remain incomplete.

macOS development packages are ad-hoc signed and not notarized. Only the native host
actually validated by the build is covered; older macOS versions are not promised.
Designer smoke coverage checks boot, one document edit, Monaco and absent Node globals;
it does not establish all designer, collaboration, GPU/LLM or worker feature coverage.
