# ADR 0183: Native per-view canvas backgrounds

Status: implementation; exact acceptance is recorded in PROGRESS.

Code-OSS supplies BrowserWindow.backgroundColor during its first window creation.
A JS-only getter or Qt palette would accept that option while the renderer still
painted white. The implementation must reach actual canvas painting and clearing,
retain author CSS and avoid a Ladybird patch stack.

Main ESM parses the supported Electron opaque color formats and validates native
reads. Constructor RGB enters the native create call before OS show; live setters
update a bounded native per-view registry and generation. Native viewport fallback
uses that registry through an owned ViewImplementation source composition. The
upstream Qt WebContentView is final and remains unchanged; owned helper functions
configure the real widget rather than introducing a fake subclass.

The parent sends complete canvas snapshots in metadata-only records through its
existing trusted BiDi session-state channel. Each record has empty source, one
actual native context handle and only integer generation/RGB arguments. Helpers
validate the bounded snapshot atomically and strip records before any preload
processing. The separate existing preload budget remains intact. There are no
new principal-world globals, style mutations or engine endpoint headers.

Renderer policy supplies a top-level context's base color to the actual document
background and display-list/canvas consumers. Author backgrounds are painted above
the base; ordinary iframe painting and unconfigured upstream behavior remain
unchanged. Changes invalidate real recording/repaint state. View context identity
persists across navigation, while generation checks reject stale/conflicting data.

The native probe uses exact production objects and an offscreen Qt plugin. It
chains the public accepted-frame callback without replacing presentation behavior,
then reads committed CPU pixels through the public native tab-preview path.
Constructor/readback tests alone do not accept the renderer path. Initial probes
had no frames while hidden, then read Qt chrome instead of the separate macOS
CALayer. Correct visibility and bitmap capture were required; the actual native
canvas path subsequently passed. Keep that diagnostic history without claiming
a renderer defect or physical CALayer acceptance.

The existing CPU bitmap/Cocoa path is opaque, so partial alpha fails explicitly.
Renderer configuration is asynchronous; a zero-flash guarantee and physical GPU/
all-platform acceptance require separate evidence. This slice does not establish
a working Code-OSS workbench merely by accepting its constructor option.
