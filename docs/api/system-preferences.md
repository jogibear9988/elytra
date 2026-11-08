# systemPreferences

Status: **partial macOS defaults, passive notifications and media status**.
Actual Node/Mite policy and exact native Host/provider checks pass. Genuine native
queries match direct OS framework observations. An opt-in isolated test also passes
real typed defaults writes/readback in a unique owned suite and process-local
notification delivery, removal and stop. No personal preference domain is changed,
no permission is requested and no physical sleep/lock or distributed/workspace event
is triggered. Native Windows/Linux providers are unavailable.

`systemPreferences` is a main-process singleton exported by `firefly`, including
through the explicit Electron compatibility alias. It is absent from ordinary
renderer capabilities and standalone Mite. The supported methods can be called
before app readiness and fail after application exit.

```js
import { systemPreferences } from 'firefly';

const style = systemPreferences.getUserDefault('AppleInterfaceStyle', 'string');
const id = systemPreferences.subscribeNotification(
  'AppleInterfaceThemeChangedNotification',
  (event, userInfo, object) => console.log(event, userInfo, object)
);

// When the observation is no longer needed:
systemPreferences.unsubscribeNotification(id);
```

## User defaults

| Method | Supported contract |
| --- | --- |
| `getUserDefault(name, type)` | Synchronous string or boolean from the native defaults provider. |
| `setUserDefault(name, type, value)` | Synchronous `undefined`; requires a value of the exact requested type. |

Only `string` and `boolean` types are supported. Other documented Electron types
fail explicitly, including integer, floating-point, URL, array and dictionary
defaults. The native provider uses standard NSUserDefaults: missing string keys yield an
empty string and missing booleans yield false, matching those native getters; the JavaScript
facade does not manufacture a missing native response. Native boolean and string
snapshots are checked before returning them.

Keys are nonempty strings bounded to 4096 UTF-8 bytes. String values may be empty
and are bounded to 65536 UTF-8 bytes. NUL and unpaired UTF-16 surrogates are
rejected. Setters pass a JSON scalar to native code, not a wrapper object.
Changing defaults can affect application behavior; this API does not imply that
every global preference is writable or that changes immediately affect other
processes.

## Passive notifications

The three subscription families follow the
[Electron systemPreferences API](https://www.electronjs.org/docs/latest/api/system-preferences):

| Subscribe / unsubscribe methods | Native center |
| --- | --- |
| `subscribeNotification(name, callback)` / `unsubscribeNotification(id)` | Distributed notification center. |
| `subscribeLocalNotification(name, callback)` / `unsubscribeLocalNotification(id)` | Local notification center. |
| `subscribeWorkspaceNotification(name, callback)` / `unsubscribeWorkspaceNotification(id)` | Workspace notification center. |

Subscriptions return monotonic signed-int32 IDs starting at **0**. All families
share a 64-subscription quota. A nonempty name selects one event; `null` subscribes
without a name filter. Names are bounded to 4096 UTF-8 bytes. The callback receives
`(event, userInfo, object)`. The sender is a string, with an empty string used for
senders that cannot be represented as a native string. No synthetic Electron
Event argument is inserted.

Unsubscribe returns `undefined`; unknown signed-int32 IDs do nothing. A call to
the wrong unsubscribe family also does nothing and preserves the existing
subscription. This deliberate ownership rule avoids losing a subscription while
removing an observer from the wrong native center. IDs cannot be strings,
fractions, nonfinite numbers or negative zero, and must fit signed int32.

Callbacks are retained while subscribed and released on removal, failed
admission, malformed delivery or shutdown. Native callbacks arriving during
subscription admission are copied and queued until admission succeeds; failure
discards them. At most 64 early packets may wait per subscription. The native owner queue also
bounds normal events to 128 overall and 64 per subscription; overflow retires the
affected subscription with a terminal data error instead of accumulating OS
notifications without a bound. Native observer admission has a process-wide quota
of 64, charged until actual observer removal succeeds, even across host shutdown. They preserve
order, and removal before the queued microtask prevents delivery.

Each notification supplies a detached JSON dictionary. The dictionary is bounded
to 65536 serialized UTF-8 bytes, depth 8 and 1024 nodes. Names and sender strings
have a 4096-byte budget. Cycles, nonfinite numbers, functions, accessors, unsupported
object prototypes, sparse arrays and lone surrogates fail validation. JSON
dictionary keys and string values preserve escaped NUL; notification names,
sender strings and default keys/values still reject NUL. Callback mutation
cannot modify the original native snapshot.

An invalid packet or native delivery-error frame retires only that subscription,
requests best-effort observer removal and throws
`ERR_FIREFLY_SYSTEM_PREFERENCES_DATA`; it never invents successful callback
arguments. Unknown or stale IDs are ignored. Callback exceptions propagate
without implicitly unsubscribing an otherwise valid subscription.

## Media access status

`getMediaAccessStatus(type)` accepts `camera`, `microphone` or `screen` and returns
one of `not-determined`, `granted`, `denied`, `restricted` or `unknown`. This is a
read-only status query, not a permission request or a renderer media grant.

The macOS screen preflight supplies `granted` when access is known to be available.
A negative preflight becomes `unknown`: it does not reliably distinguish an
unrequested permission from an explicit denial. Unavailable native services fail
explicitly; they do not become fabricated grants. Results belong to the current
process/responsible launcher. Packaged identity, entitlements, usage descriptions
and real capture authorization require separate acceptance.

## Unsupported APIs and lifecycle

`registerDefaults`, `removeUserDefault`, all notification-posting methods,
`askForMediaAccess`, Touch ID and accessibility prompting, colors, appearance,
animation and swipe preferences remain unsupported. Advertised EventEmitter
registration/emission methods also fail explicitly; this slice does not supply
the Windows/Linux color-change event surface. Unsupported methods/properties
throw `ERR_FIREFLY_UNSUPPORTED_API`.

Methods require their original singleton receiver. Strict input validation,
resource limits and Firefly error codes do not claim identical Electron error
classes, wording or coercion. Subscription quota/ID exhaustion throws
`ERR_FIREFLY_SYSTEM_PREFERENCES_LIMIT`. Malformed native defaults or status
snapshots throw `ERR_FIREFLY_SYSTEM_PREFERENCES_DATA`.

Application exit permanently revokes the facade, clears all callback references
before native cleanup, and removes observers best effort. Reentrant exit during
a query, write or subscription cannot return a stale successful result. Calls
after exit throw `ERR_FIREFLY_APP_EXITED`; late notification packets are ignored.
Observer removal cannot undo a default write that native code already performed.

## Validation scope

`firefly/tests/system-preferences/policy.mjs` and
`tooling/tests/test_system_preferences_policy.py` pass in actual Node and Mite
against a controlled native peer. They cover defaults/status types, strict
validation, all three notification families, detached snapshots, early callback
ordering/rollback, quotas, malformed delivery, observer removal and reentrant
shutdown. They make no actual OS preference changes or notification posts and
request no permissions. The [exact native Host fixture](../../firefly/tests/system-preferences/HostProbe.cpp)
passes the actual factory/LibJS boundary, strict JSON/Unicode, synchronous and worker
notification delivery, bounded queue overflow, callback GC, failed admission and
controller/receiver/VM retirement. Qt values are materialized before temporary
JSON containers expire; typed native values never reference destroyed wrappers.

The [exact macOS provider fixture](../../firefly/tests/system-preferences/ProviderProbe.mm)
uses controlled defaults/centers/authorization peers. Original OS preferences,
notification posting and permission functions are not invoked there. It checks
native metadata, three centers, snapshots, errors, late stop and failed-removal
quota/recovery.

The [read-only probe](../../firefly/tests/system-preferences/ReadOnlyProbe.mm)
independently compares genuine AVFoundation/CoreGraphics/defaults values and
installs/removes passive observers. It neither changes defaults nor requests
permissions or posts notifications.

The opt-in [isolated native probe](../../firefly/tests/system-preferences/IsolatedProbe.mm)
redirects only the standard-defaults factory to a real, uniquely named NSUserDefaults
suite. Production methods write/read real native scalar values, synchronize and
check a fresh store/provider; the exact owned domain is removed before passing.
A genuine uniquely named local notification in the probe process verifies actual
delivery, unsubscribe and stop. This establishes isolated-suite/local-center
behavior, not cross-process persistence, personal/default-domain changes, physical
sleep/wake or distributed/workspace delivery.

The [unavailable provider check](../../firefly/tests/system-preferences/UnsupportedProbe.cpp)
proves explicit refusal and no fabricated data. Fresh Windows/Linux acceptance,
physical system events, default-domain persistence and packaged permission/capture
behavior remain open. Non-GUI Host/unavailable gates are wired into Linux renderer
CI and release builds, with controlled/read-only macOS checks on release; fresh
hosted execution requires its own evidence.

The actual pinned Code-OSS local-desktop/NLS ready graph passes this export and
now also passes fork and the protocol service; it next fails importing native
@vscode/policy-watcher. The separate unchanged IPC Client/Server component passes genuine Mite
fork/JSON messages. Neither gate accepts a complete watcher, extension host,
VS Code window or open/edit/save workflow. See [VS Code status](../compatibility/vscode.md).
