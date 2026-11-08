# mite:policy-watcher

Status: Mite-specific extension with local macOS ARM64 acceptance. It is independent
of Firefly, LibWeb, Node and N-API. Windows and Linux providers currently throw
ERR_MITE_UNSUPPORTED_API; import alone performs no OS observation.

```js
import { createWatcher } from 'mite:policy-watcher';

const watcher = createWatcher('com.example.editor', {
  OrganizationURL: {type: 'string'},
  DisableFeature: {type: 'boolean'},
  ManagedSetting: {type: ['string', 'number', 'boolean']}
}, (updates) => {
  console.log('Observed policy keys:', Object.keys(updates));
});

setTimeout(() => watcher.dispose(), 30_000);
```

## createWatcher(productName, definitions, callback[, options])

Returns an object with idempotent dispose(). ESM default/named exports and CommonJS
require('mite:policy-watcher') share the same realm-local instance. This is a Mite
extension, not a Node builtin or general native-addon loader.

productName selects the actual CFPreferences application domain. Definitions map
nonempty names to objects with a type of string, number, boolean, or a nonempty
array containing those types. Duplicate union members are normalized; additional
definition metadata is ignored, matching the pinned package's type-only reader.
Unknown types are rejected. Optional registryPath is accepted only when empty or
absent; nonempty/custom options fail explicitly.

The callback first runs asynchronously after a genuine native snapshot, including
an actually observed empty set. No synthetic empty snapshot unblocks initialization.
After a changed snapshot, updates contains all present values plus undefined for
removed keys. Identical snapshots do not notify. Updates have a null prototype;
consumers should use Object.keys/Object.entries or Object.hasOwn. Notifications may
coalesce intermediate states; they are not an event log of individual writes.

The native observer keeps Mite alive until disposal. dispose() removes callback
ownership immediately, stops and drains the native observer, and lets owner-loop
handle storage close asynchronously. Queued or late data cannot invoke that callback
after disposal. Callback errors use the ordinary uncaught-error path.

## macOS policy semantics

The provider uses CFPreferencesCopyAppValue for exact CFString, CFBoolean and
CFNumber values and observes /Library/Managed Preferences with a serial FSEventStream.
It never writes preferences. The first query and later refreshes run on the native
worker; only bounded plain snapshots return to Mite's owner event loop.

The compatibility baseline is @vscode/policy-watcher 1.7.0. Its scalar definitions
read preferences without a forced-policy check; union definitions, including
one-member arrays, additionally require CFPreferencesAppValueIsForced. This
distinction is preserved. Numbers use successful signed-long-long CF conversion,
then JavaScript Number representation; unsupported/inexact CF conversions are
absent. String conversion follows the pinned addon's UTF-8 C-string semantics,
including termination at an embedded NUL. Other CF types are absent.

## Bounds and errors

At most 16 native watchers, including closing handles, and 256 definitions per
watcher are accepted. Product/key names are bounded to 4096 UTF-8 bytes, string
values to 64 KiB, and definitions/snapshot JSON to 1 MiB. Malformed Unicode, NUL
in names, invalid types and malformed native metadata are refused. Rapidly replacing
many watchers may need an owner-loop turn for closing handles to drain.

Invalid arguments use ERR_INVALID_ARG_TYPE/ERR_INVALID_ARG_VALUE, bounds use
ERR_MITE_POLICY_LIMIT, unavailable platforms use ERR_MITE_UNSUPPORTED_API, and
native startup/read failures use ERR_MITE_POLICY_PROVIDER. A native read failure
is latched, retires the watcher and enters Mite's uncaught-error path; it cannot be
replaced by empty successful data. Invalid native snapshots use ERR_MITE_POLICY_DATA.

## Code-OSS adaptation and evidence

The explicit [Code-OSS build adapter](../guides/code-oss-adaptation.md#native-policy-package-adapter)
redirects only the pinned 1.7.0 loader when process.versions.mite identifies Mite.
Ordinary Node/Electron retain their original addon branch. Source, package, adapter
and output hashes are recorded; original prepared dependencies and licenses remain
unchanged. General .node/N-API loading is still unavailable.

Controlled Node/Mite facade tests, exact native Host/GC/error subprocesses and the
real CF/FSEvents provider with controlled peers pass locally. Passive public-provider
and unchanged Code-OSS NativePolicyService/NativeManagedSettingsService consumers
also initialize and dispose against a unique unused domain without preference
writes. These checks do not establish actual MDM change delivery, Windows/Linux
availability or a working Code-OSS workbench. See [ADR 0176](../architecture/0176-native-policy-watcher.md).
