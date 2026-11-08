# Sandboxed preload process

Availability: Firefly isolated top-level CommonJS preload; partial implementation.
The native main-process [Mite process API](process.md) is a separate object.
This object gives trusted preloads renderer identity and launch data without
providing filesystem, process creation, Node/V8 or Mite bindings to the page.

## Properties

| Property | Value and scope |
| --- | --- |
| `pid` | Actual owned WebContent helper OS process ID, also available to main through `webContents.getOSProcessId()` |
| `platform`, `arch` | Native build platform (`darwin`, `linux`, `win32`) and architecture (`arm64`, `x64`); listing values does not establish platform acceptance |
| `execPath` | Actual helper executable path, captured before its sandbox initializes |
| `type` | `renderer` |
| `sandboxed`, `contextIsolated`, `isMainFrame` | `true`; the supported product preload runs only in the isolated, sandboxed top-level document |
| `version` | Framework version with a `v` prefix, currently `v0.1.0` |
| `versions` | `firefly` framework version and pinned `libjs` / `ladybird` commit hashes; no fabricated `node`, `v8`, `chrome` or `electron` versions |
| `argv` | Fresh mutable array containing `execPath`, followed by the window's `additionalArguments` in order |
| `env` | Fresh mutable string-valued object from the main OS environment captured at window construction |

Identity properties are enumerable, nonwritable and configurable. `argv`, `env`
and `versions` are ordinary mutable source-realm containers. Mutating them changes
only that preload's copy. Reload creates fresh containers from the original window
snapshot; a new window captures the current main environment. Invalid OS environment
UTF-8 is normalized using replacement characters. Own environment keys are defined
as data properties, including names such as `__proto__`.

No `process` is installed in the principal webpage, initial `about:blank` or child
frames. Publish selected metadata through [contextBridge](context-bridge.md), and
expose narrow fixed IPC actions for privileged operations.

## webPreferences.additionalArguments

```js
// Main process: use an absolute path to the bundled CommonJS preload.
const window = new BrowserWindow({
  webPreferences: {
    preload: preloadPath,
    additionalArguments: ['--vscode-window-config=workspace-1']
  }
});
```

```js
// Authored preload.mjs; bundle before launch.
import { contextBridge, ipcRenderer } from 'firefly';
const configKey = process.argv.find(arg => arg.startsWith('--vscode-window-config='));
contextBridge.exposeInMainWorld('workspace', {
  platform: process.platform,
  loadConfiguration: () => ipcRenderer.invoke('app:configuration', configKey)
});
```

The constructor snapshots the array. Empty strings, Unicode, spaces, `=` and
repeated values are retained. Values must be own string array entries: at most
64 arguments, 4,096 UTF-8 bytes per argument, 16 KiB of argument content in total,
no NUL or malformed UTF-16. Invalid types throw `ERR_INVALID_ARG_TYPE`; invalid
values/budgets throw `ERR_INVALID_ARG_VALUE`. Nonempty arguments without a configured
preload throw `ERR_FIREFLY_UNSUPPORTED_API`.

Environment plus argument content is bounded to 64 KiB/window, with at most
1,024 environment entries. Excess environment/window metadata throws
`ERR_FIREFLY_PRELOAD_TOO_LARGE`. Native environment-read failure throws
`ERR_FIREFLY_PRELOAD_READ_FAILED`. The helper independently validates the versioned
metadata schema and budgets. Source and process data each retain separate 1 MiB
aggregate limits across at most 16 configured preload windows.

## Differences and unsupported operations

[Electron's sandbox process](https://www.electronjs.org/docs/latest/api/process#sandbox)
provides a restricted subset of its Node-derived process object. Firefly provides
its own truthful metadata subset. Its logical `argv` deliberately excludes internal
helper/control switches. Application arguments travel through validated per-window
metadata rather than the helper OS command line; they cannot toggle native helper
options. Electron documents `additionalArguments` in
[WebPreferences](https://www.electronjs.org/docs/latest/api/structures/web-preferences).

`process.on()` and `process.getProcessMemoryInfo()` throw
`ERR_FIREFLY_UNSUPPORTED_API`; event delivery and memory measurements are unfinished.
These raw native methods cannot be exported through contextBridge. Other process
methods, including `binding`, `cwd`, `exit`, signal and process creation APIs, are
absent. `Buffer` and filesystem loaders are not installed. This is not full Node
or Electron process compatibility; Code-OSS's complete preload still needs other
APIs and its workbench does not yet run.

## Validation

Local macOS Release acceptance uses `check-firefly-isolation` for native schema,
exact/overflow byte and count budgets, and `check-firefly-preload-process` for the
actual production helper: two hidden windows, reload, copied argv/env, early config
IPC, source userEnv merge, truthful PID/executable/version data, independent live OS
parent checks and page/child-frame isolation. The shared supported behavior also
runs against a real Electron reference. Firefly-specific limits, logical argv,
metadata version identity and explicit unsupported methods are separate assertions.
The measured Electron macOS reference rearranges switches and positional arguments
and omits the empty argument; its common gate checks nonempty argument values and
multiplicity. Firefly separately verifies exact order and empty-string preservation.
The Electron reference also does not equate execPath with argv[0].
Fresh Linux and native Windows acceptance of this slice remain unverified.
