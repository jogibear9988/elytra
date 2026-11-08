# Notification

Status: **partial main-process API; actual OS delivery remains untested**.
A macOS UserNotifications provider is implemented, including authorization,
scheduling, removal and response routing. Production JavaScript policy tests
pass in Node/Mite with a controlled provider. A read-only native bare-CLI probe
passes with support reported false. No permission dialog, valid OS delivery,
visible banner, click or action-button interaction has been accepted.
Linux/Windows providers report unsupported behavior explicitly.

`Notification` is an EventEmitter exported by `firefly`, including through the
explicit Electron compatibility alias. It does not expose native notifications
to ordinary renderer scripts or standalone Mite. The renderer Web Notifications
API is a separate capability and is not provided by this class.

```js
import { app, Notification } from 'firefly';

app.whenReady().then(() => {
  if (!Notification.isSupported()) return;

  const notification = new Notification({
    title: 'Export complete',
    body: 'The document is ready.',
    silent: true,
    actions: [{ type: 'button', text: 'Open' }]
  });
  notification.on('failed', (_event, message) => console.error(message));
  notification.on('action', (_event, index) => console.log('Action', index));
  notification.show(); // May request macOS notification authorization.
});
```

## Static method and construction

`Notification.isSupported()` returns a boolean. Before app readiness and after
app exit it returns false without entering the native provider. On macOS the
provider checks the OS version, an eligible `.app` bundle identity and notification
delegate availability. A bare command-line executable intentionally returns
false. A true result is not proof of authorization, signing validity, successful
scheduling or visible presentation.

`new Notification([options])` requires app readiness. Construction validates and
owns JavaScript values; `show()` performs native submission. Option/action getters
are read into snapshots, and readiness/exit is checked again after getters run.

| Option / mutable property | Default and supported values |
| --- | --- |
| `title` | Empty string; NUL-free string, at most 4,096 UTF-8 bytes. |
| `body` | Empty string; NUL-free string, at most 65,536 UTF-8 bytes. |
| `silent` | False; boolean controlling notification sound. |
| `actions` | Empty array; at most four `{ type: 'button', text: string }` actions. Text must be NUL-free and at most 1,024 UTF-8 bytes. |

Provided null title/body values are invalid; omitted values use the defaults.
Action arrays and entries are copied on construction and assignment. Reading
`actions` returns a fresh copy. Rejected setters preserve the previous valid
value. Mutating properties affects the next submission, not an already submitted
native snapshot.

Other constructor options and action fields fail with
`ERR_FIREFLY_UNSUPPORTED_API`. This includes icons, subtitles, replies, custom
sounds, urgency/timeout settings, custom close text, toast XML, persistent IDs
and notification groups. The known unsupported instance properties `icon`,
`subtitle`, `hasReply`, `replyPlaceholder`, `sound`, `urgency`, `timeoutType`,
`closeButtonText` and `toastXml` also reject reads and assignments. Notification
history, activation handlers, reply/selection actions and full Electron property
coverage remain unavailable.

## show() and close()

Both return undefined on their normal paths.

`show()` snapshots the supported properties and starts a fresh owned generation.
Calling it again on the same object retires the previous generation, requests
native removal and emits its managed `close` event before submitting a replacement.
A close listener can call `show()`, `close()` or exit the app; generation checks
preserve that listener's decision. A replacement cannot be overwritten by the
outer call resuming afterward.

`close()` detaches the current generation before requesting native removal. For
an active generation, its managed `close` event is synchronous unless shutdown or
reentry has superseded it. Closing an inactive generation is a no-op. This is an
explicit Firefly lifecycle policy: Electron's programmatic close-event behavior
and timing vary by platform, and no native Electron comparison was run here.

Native removal and shutdown are best effort. Pending and delivered requests are
removed by their owned identifiers; a late add completion attempts removal again.
An already displayed OS notification cannot be synchronously rolled back. Closing
an object revokes JavaScript delivery authority, not an observation that an alert
has disappeared from the user's desktop.

## Events

| Event | Current arguments and meaning |
| --- | --- |
| `show` | `(event)`, once per generation when the native request was accepted. **This does not prove a banner was visible.** |
| `click` | `(event)` for the owned default user response. Terminal for that generation. |
| `action` | `(event, index)` for a configured button. `event.actionIndex` equals the zero-based index. Terminal. |
| `close` | `(event)` for native dismissal or the managed explicit/replacement close described above. |
| `failed` | `(event, message)` with a failure string. Terminal. Native submission exceptions are delivered asynchronously through this event. |

Terminal events retire generation authority before invoking listeners. Duplicate
acknowledgements and late events from replaced, closed or stopped generations are
ignored. Invalid native event types, action indices or oversized failure messages
become a terminal `failed` event. Closing before a queued submission failure runs
revokes that failure delivery.

App exit closes active native generations and suppresses their user callbacks.
Native close/support boundaries are checked again after reentry, so an app exit
cannot produce a stale support result or a terminal callback afterward.

## Limits and errors

At most 64 generations may be active at once. Retiring one releases its
JavaScript slot. Generation tokens are monotonically increasing positive integers
bounded by signed `INT_MAX`; user code cannot choose or reuse them. Native work
also has bounded admission and action-category bookkeeping.

| Error code | Meaning |
| --- | --- |
| `ERR_FIREFLY_NOT_READY` | Constructor or `show()` called before readiness. |
| `ERR_FIREFLY_APP_EXITED` | Constructor or `show()` attempted after exit, including exit during reentry. |
| `ERR_INVALID_ARG_TYPE` | Invalid options/receiver, non-string or NUL-containing text, non-boolean silent value, or malformed action data. |
| `ERR_OUT_OF_RANGE` | Text byte budget or four-action limit exceeded. |
| `ERR_FIREFLY_UNSUPPORTED_API` | Unsupported option, property or action capability. |
| `ERR_FIREFLY_NOTIFICATION_LIMIT` | Active-generation quota or token range exhausted. |

Native scheduling/authorization failures are exposed as `failed` messages rather
than a promise result. The source implementation does not claim that a successful
`show()` return means the OS accepted or displayed the notification.

## Platform and validation scope

The macOS provider uses UserNotifications on macOS 11 or later. Native support
preflight reads bundle/OS/delegate eligibility and does not request authorization,
schedule alerts or replace another delegate. Actual `show()` may request access.
Packaging, appropriate application identity and OS policy still determine whether
notifications can be delivered; valid bundled-app delivery is not tested here.

Linux/Windows currently return false from `isSupported()`; submission reports a
`failed` event instead of simulating display. Fresh native platform implementations
and acceptance remain required.

- [Policy fixture](../../firefly/tests/notifications/policy.mjs): actual Node/Mite
  execute the production API factory with controlled support/submission/close
  callbacks. Validation, field snapshots, button payloads, generations, failure
  delivery, quotas and shutdown/reentry cases pass. This does not call an OS
  notification provider.
- [Native provider probe](../../firefly/tests/notifications/ProviderProbe.cpp):
  the actual bare executable returns false at identity preflight. It does not
  request authorization, schedule a notification or mutate the native delegate.
- [Native host fixture](../../firefly/tests/notifications/HostProbe.cpp): a
  controlled provider peer targets the production dispatcher. Validation, worker
  routing, de-duplication, quotas and shutdown/VM teardown pass; this does not
  establish real OS delivery.
- [Native provider state fixture](../../firefly/tests/notifications/ProviderStateProbe.mm):
  the exact macOS backend runs against isolated system peers. Original bundle/OS
  singleton implementations are never invoked. Authorization/coalescing, cancel,
  native responses, late-add cleanup and category transaction ownership pass.
  This is state-machine acceptance, not actual OS authorization or delivery.
- Electron documentation/source informed API shape; no Electron Notification
  constructor, support probe or delivery operation was invoked for this slice.

Permission flows, bundled-app scheduling, visible presentation, user clicks,
action buttons, category interoperability and cleanup need a separate opt-in
native acceptance. See the [Electron reference](https://www.electronjs.org/docs/latest/api/notification)
for the broader upstream API.
