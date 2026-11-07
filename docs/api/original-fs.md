# original-fs

Status: partial Firefly main-process module. Local macOS ARM64 tests run the same
ESM fixture in the real Firefly starter and Electron 45.2.3 console mode, plus a
non-GUI embedded Mite host with actual GC. Native Linux and Windows x64 also pass
the host/standalone gate at 678c85e (all 68 runtime suites). Linux product execution
remains pending; Windows desktop remains unavailable.

```js
import fs, {readFileSync, promises} from 'original-fs';
const bytes = readFileSync('/path/to/physical-file');
const text = await promises.readFile('/path/to/config.json', 'utf8');
console.log(bytes.length, text);
```

Firefly registers this module before application entry. CommonJS createRequire
and ESM share the same raw exports object; named exports cover Mite's implemented
fs subset. require.resolve('original-fs') returns original-fs. The module is local
to the trusted main-process loader; it does not enable Node/filesystem APIs in
preloads or page content.

## Raw files and independent facades

The exports object and its promises object are distinct from node:fs. Replacing
ordinary fs.readFileSync, fs.writeFileSync, fs.accessSync or fs.promises.readFile
does not replace the raw functions. The raw factory's append/exists and FileHandle
methods also retain their own raw facade references, rather than indirectly
calling the publicly patched fs object.

Both facades use the same native filesystem host and event loop. Actual numeric
descriptors work across them: opening in one and reading in the other does not
require copying file contents or creating a second backend. FileHandle lifetime
and descriptor ownership still follow the [filesystem contract](mite.md); do not
externally close a descriptor still owned by an open FileHandle.

This is not a deeply immutable filesystem snapshot. Shared supporting constructors,
native constants and their full identity/prototype behavior are not established
as Electron-compatible. The same unsupported fs APIs, errors and platform limits
as Mite's fs subset remain unsupported here.

## ASAR and standalone Mite

Electron's original-fs bypasses its ASAR virtual directory handling. Firefly does
not implement that virtual filesystem yet: both ordinary fs and original-fs use
physical OS paths. The raw module reads a physical .asar file as bytes, but this
does not enable archive-entry reads, ASAR-packed Code-OSS dependencies, require
from an archive or process.noAsar. Continue using unpacked application trees until
archive routing has its own implementation and acceptance.

Standalone Mite does not register original-fs and gains no Firefly dependency.
Without an installed package it rejects the import with ERR_MODULE_NOT_FOUND;
with a local npm package it performs ordinary package resolution. Firefly's
trusted registration takes precedence over a local package of the same name.
node:original-fs is not a Node builtin alias. Host modules also retain the
documented [registration/cache policy](../architecture/0023-host-modules.md).

## Verification

The fixture checks independent Python-verified bytes/SHA-256, callback/Promise/
sync IO, URL paths, raw .asar bytes, both descriptor directions, FileHandle use,
patch isolation and actual embedded-host collection. It runs no BrowserWindow.
The macOS native starter uses its shipped Cocoa plugin and calls app.quit at the
end; the minimal Qt build has no offscreen plugin. check-firefly-original-fs runs
only this focused case, and check-firefly includes it in release acceptance.
The standalone host/CLI gate is the 68th CTest suite on native platform CI.

Reference: [Electron ASAR and original-fs](https://www.electronjs.org/docs/latest/tutorial/asar-archives#treating-an-asar-archive-as-a-normal-file).
