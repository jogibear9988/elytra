# Canonical frame identities independent of preload IPC

VS Code's unchanged webRequest filters inspect frame URL, ancestry and internal
renderer identity, and compare that identity with webContents.mainFrame. The old
preload-only sender snapshot existed only after IPC and had no genuine process or
parent metadata. It cannot authorize resource requests before preload starts.

## Decision

Add a native registry of active canonical documents and frame nodes. Derive metadata
from the owned native view/traversable, actual document host and live ancestor
containers. Use the engine's managed child identity for processId and the separate
native PID for osProcessId. Allocate document/routing IDs monotonically, and keep
frame-node IDs tied to the actual canonical node across document changes.

Native records contain weak canonical document/navigable identities. Resolution
rechecks current document, host, process and ancestry. Snapshots exclude unhosted
placeholders instead of inventing metadata. Malformed identities and exhausted
budgets fail explicitly. No page URL or JS-supplied contents ID creates authority.

Maintained ESM presents read-only live proxies and caches wrappers weakly. Every
property access revalidates the native record. Lookups and admitted IPC senderFrame
resolve to these same proxies. IPC cancellation owns its pending replies; merely
binding a preload channel must not destroy a canonical frame. Real navigation,
host loss, close and shutdown retire unavailable document identities.

This is a document-scoped subset, not a Chromium frame-host emulator. Frame routing
methods/events, BFCache parity and other unsupported operations remain explicit.
A metadata object grants no file access or other renderer capability. Future
webRequest pending requests still require their own document/navigation generation,
client authentication and lifetime checks; accepting a JS proxy is insufficient.

## Integration and acceptance

The native registry and main bridge use existing public canonical engine classes.
Ladybird stays unmodified; no engine IPC/bootstrap header expansion is needed.
Standalone Mite acquires no Firefly/LibWeb/Qt dependency.

Use controlled Node/Mite policy tests and one actual offscreen frame-tree batch:
identity before application IPC, same/cross-origin ancestry, process semantics,
hash retention, new-document retirement, genuine IPC aliases and owner closure.
The hidden Electron reference shares the applicable checks; wait for actual frame
retirement after window closure. See [the API contract](../api/web-frame-main.md)
and [the progress log](../PROGRESS.md) for exact accepted scope and remaining work.
