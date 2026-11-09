# session

Status: partial default-session protocol, document-bound permissions and request policy.
Import from firefly, or the explicit Electron alias. ESM/CommonJS share the same
main-process object. See [the compatibility matrix](../compatibility/firefly-0.1.md)
for accepted platform scope.

## session.defaultSession

Available after app readiness; earlier access throws ERR_FIREFLY_NOT_READY. Returns
the existing frozen default desktop browsing-session facade. Its protocol property
is the same singleton as the top-level protocol export. An owned
webContents.session returns this same default Session; custom sessions remain
unsupported. Access on destroyed contents throws.

```js
import { app, protocol, session } from 'firefly';
await app.whenReady();
console.log(session.defaultSession.protocol === protocol); // true

session.defaultSession.setPermissionCheckHandler(() => false);
session.defaultSession.setPermissionRequestHandler((_contents, _permission, decide) => {
  decide(false);
});
```

The policy routes below cover document geolocation and notifications; media grants
are denied. Clipboard remains governed by its separate native gesture policy.

The [protocol reference](protocol.md) describes registration, modern Response
handlers, asynchronous file callbacks, file interception, ownership and limits.
The [webRequest reference](web-request.md) describes request cancellation, response
header replacement, canonical frames, bounded callbacks and remaining transport gaps.

## setPermissionCheckHandler(handler)

Accept a synchronous function or null. The function receives
`(webContents, permission, requestingOrigin, details)` and returns a boolean.
Only a literal true allows a supported query. False, another return type, a busy
main VM or an unavailable owner denies. A thrown callback denies and follows the
main runtime's exception-reporting policy. Promises are not permission decisions.

The native process derives webContents identity, requestingOrigin and details from
its actual active document, never from a page-supplied URL. Details include
requestingUrl, isMainFrame and embeddingOrigin. Microphone checks use the Electron
name media with mediaType audio; capture remains unavailable and is denied even
if the application callback returns true. Geolocation and notifications have
separate document-level policy. Check callbacks run before renderer permission
caches, including on fresh queries after handler replacement.

Cross-origin frames require the engine's secure-context and Permissions Policy
checks before application policy is consulted. Notification authorization also
retains the native same-top-origin constraint. Worker/ownerless documents are
not accepted as a window permission requester. Separate out-of-process worker
queries are not integrated with these handlers and can retain upstream default
status; do not infer worker permission support from window query acceptance.

## setPermissionRequestHandler(handler)

Accept a function or null. The function receives
`(webContents, permission, callback, details)`. Call callback with a boolean once.
Details contain the native requestingUrl and isMainFrame; microphone requests add
mediaTypes containing audio. The request path is separate from permission checks:
a denied check must not suppress an otherwise valid application request callback.
Configure both handlers for a consistent application policy.

An asynchronous decision belongs to its original active document and policy
generation. Native code revalidates both before accepting it. Duplicate or stale
callbacks cannot grant. A nonboolean decision first denies, then throws
ERR_INVALID_ARG_TYPE. A handler exception denies an unsettled request, then follows
the ordinary uncaught callback policy. Missing callbacks time out after 30 seconds.

Application grants for supported permissions belong to one exact native document;
they never write an origin-wide browser permission record. Replacing either
handler, navigation, owner close and shutdown revoke pending ownership and grants.
Native admission is limited to 16 pending requests per webContents and 64 in the
application, with at most 256 document grant records. Canonical URL/origin fields
are limited to 64 KiB UTF-8 and serialized metadata to 256 KiB before native
callback entry or request admission. Closed/replaced frame records
are also pruned. Exhaustion denies; native tokens and generations never wrap.

Media capture grants remain explicitly unsupported: callback(true) for media
settles the native request as denied and throws ERR_FIREFLY_UNSUPPORTED_API.
Camera, display capture, pointer lock, local fonts and clipboard permission routing
are not supplied by these setters. An application callback never grants an OS
privacy entitlement or implements a missing device backend.

## Reset, lifecycle and differences

- null removes the corresponding handler. Firefly's default and reset behavior is
  denial. Electron's default policy is broader; this is an intentional security
  difference while the session implementation remains partial.
- Replacing any handler cancels already admitted pending requests. Electron can
  retain pending callbacks when replacing a non-null request handler. Requests
  still waiting for native admission use the policy current at that admission.
- Check callbacks cannot enter a busy main VM or a synchronous native dialog;
  these checks deny instead of waiting for reentry. Native replies also revalidate
  document and generation after the callback's Promise/tick checkpoint.
- Reentrant configuration rejects with ERR_FIREFLY_SESSION_REENTRANT. Stop during
  configuration revokes the reserved generation and rejects the registration.
  Closed setters throw ERR_FIREFLY_SESSION_CLOSED; invalid handler types throw
  ERR_INVALID_ARG_TYPE. Failed configuration leaves old JS closures revoked.
- Current queries are checked dynamically. Complete PermissionStatus change-event,
  storage/persistence, worker permission and OS/device delivery parity are separate
  requirements. Permission policy acceptance alone does not establish location
  results, notification delivery or microphone/camera access.

fromPartition, fromPath, custom BrowserWindow partitions/sessions, cookies,
webRequest, display-media request handlers, download APIs and wider session methods
remain unimplemented. A requested custom partition is never silently routed through
the default session. The [architecture decision](../architecture/0178-session-permissions.md)
describes the native boundary and acceptance gates.

Reference: [Electron session API](https://www.electronjs.org/docs/latest/api/session).

## Validation scope

Local macOS Release acceptance covers genuine main/same/cross-origin renderer
queries, real geolocation request-handler denial, replacement/reset, same-origin
window isolation and stale replies after navigation/close. The shared query/denial
path passes Electron 45.2.3. Twelve native boundary cases use an actual owned
document and the production receiver/broker, including controlled grant/revocation
without any device API. Controlled factory cases also pass Node and Mite.

This does not accept notification request activation/delivery, actual location
results, elapsed-time deadline behavior under every scheduling condition, microphone
capture, separate worker queries or native Linux/Windows execution.
