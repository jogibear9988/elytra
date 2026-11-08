# Firefly CLI and Electron import compatibility

```sh
firefly [--electron-compat] main.mjs [application arguments...]
firefly --help
```

Native source runtime: macOS ARM64 and experimental Ubuntu 24.04 x64 X11.
Windows desktop remains unavailable. Mite's independent [CLI](cli.md) does not
provide these flags or register desktop modules.

## Explicit import alias

By default, import/require('firefly') loads the native API; 'electron' keeps normal
npm resolution. Place --electron-compat **before** the entry filename to register
'electron' as another name for the exact same main-process API object. ESM
default/named exports and CommonJS share app, BrowserWindow and the other actually
implemented exports. The host alias takes precedence over an installed npm
electron package. It does not create unsupported exports or implement Chromium,
Node or V8. Runtime versions still describe Mite/LibJS.

The option is stripped from process.argv; arguments after the entry are forwarded
unchanged. Thus an application argument named --electron-compat does not enable
the host policy. Missing entries, repeated or unknown host options are refused
before desktop initialization. This is current source functionality, not part of
the older published Alpha 2 binaries.

## Sandboxed preloads

The same executable policy is passed through trusted native preload metadata.
When enabled, require('electron') and require('firefly') return the same frozen
preload facade: contextBridge and the current ipcRenderer subset only. They do
not return the main-process API. Node filesystem/process modules and Electron
subpath imports remain rejected by the preload loader. Default preloads still
reject require('electron').

Page JavaScript never receives require, process, Buffer or the host policy object.
Existing top-level document selection, isolated-world ownership and revocation
remain enforced. This does not enable ES-module preloads, nodeIntegration or
the wider Electron preload API surface. Main-process 'electron/main' and other
subpaths are not aliased.

## Packaging

Both native packagers accept --electron-compat. The generated native launcher
places the option before the recorded application entry; user arguments stay
after it. BUILD.json records electronCompat. Without the flag, packaging retains
default lookup behavior. Packaging checks that the selected SDK recognizes the
option using its non-GUI help path, refusing an older incompatible runtime.
The alias does not install dependencies or remove an
unused npm Electron dependency; keep build-only tools in devDependencies and use
the existing production payload policy.

```sh
python3 tooling/package_linux.py --help
python3 tooling/package_macos.py --help
```

These are options to the existing platform packagers, not a Windows package
implementation or a complete Code-OSS distribution recipe.

## Evidence and limits

Local macOS tests use actual Firefly/owned WebContent with hidden windows for
default/opt-in main and preload modes, npm precedence, ESM/CommonJS identity,
unchanged application arguments, frozen limited imports and absent page privileges.
Native launcher tests compile real macOS/Linux launcher sources and check relocation,
UTF-8/spaces, option placement and exec PID ownership. The Linux native gate
passes at 5735b91 in run 42386340939. Pure argument tests need no GUI/VM.
Whole flagged application archives still need separate platform acceptance.

The actual pinned Code-OSS entry now advances through cache setup and 'electron'
resolution in the Firefly host; with the frozen root production dependencies, its current diagnostic fails on
unsupported Chromium enable-features (exit 1) without the engine overlay. The explicit
--libjs-engine application adaptation advances to unsupported protocol privilege allowServiceWorkers (exit 1),
after passing the registerHooks/promisify imports. That is an incomplete startup trace,
not a desktop launch. See the [dependency preparation guide](../guides/code-oss-adaptation.md#prepare-actual-root-production-dependencies). See
[Code-OSS support](../compatibility/vscode.md) and [ADR 0107](../architecture/0107-electron-import-policy.md).
