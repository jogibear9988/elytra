# desktopCapturer

Status: **partial metadata API; native source enumeration is implemented but untested**.
The macOS ScreenCaptureKit provider and asynchronous main-process dispatcher are
implemented. Node/Mite policy tests and a controlled native dispatcher test pass;
a native permission preflight also passes without requesting access. Valid OS
source enumeration, permission dialogs, screenshots and media streams have not
been accepted. Linux/Windows providers explicitly report unavailability.

This is a main-process export from `firefly`, also available through the explicit
Electron compatibility alias. It is absent from the ordinary renderer capability
surface and standalone Mite.

```js
import { app, desktopCapturer } from 'firefly';

app.whenReady().then(async () => {
  // A valid macOS request may cause the OS to request Screen Recording access.
  const sources = await desktopCapturer.getSources({
    types: ['screen', 'window'],
    thumbnailSize: { width: 0, height: 0 },
    fetchWindowIcons: false
  });
  console.log(sources.map(({ id, name }) => ({ id, name })));
}).catch(console.error);
```

## getSources(options)

Returns `Promise<DesktopCapturerSource[]>`. Options are read before lifecycle
admission; a valid request requires a ready application. The application state
is checked again after option getters run, so a getter that exits the app cannot
start a native operation.

| Option | Current contract |
| --- | --- |
| `types` | Required array containing only `screen` and/or `window`. At most 64 entries; duplicates select the same type once. An empty array filters the result to an empty list after enumeration. |
| `thumbnailSize` | Optional object with integer `width` and `height`, each in 0..16384. Either dimension must be zero in the current subset. |
| `fetchWindowIcons` | Optional boolean, default false. True is explicitly unsupported. Null and other non-booleans are invalid. |
| Other options | Rejected with `ERR_FIREFLY_UNSUPPORTED_API`. |

The Electron default thumbnail size is 150×150. Firefly retains that default
when checking options, then refuses it because pixel capture is not implemented.
Supply an explicit zero dimension, such as 0×0, 0×150 or 150×0. Omitting
`thumbnailSize` does **not** silently disable thumbnails. This restriction is
narrower than the [Electron API](https://www.electronjs.org/docs/latest/api/desktop-capturer).

Malformed options reject asynchronously with `Error('Invalid options')`, without
an error code. This behavior was measured in Electron 45.2.3 for malformed
`types` before and after app readiness. Valid requests before readiness reject
with `ERR_FIREFLY_NOT_READY`; valid requests after application exit reject with
`ERR_FIREFLY_APP_EXITED`.

## Source records

Each returned record contains:

| Property | Current value |
| --- | --- |
| `id` | Canonical OS-derived string: `screen:<display-id>:0` or `window:<window-id>:0`. IDs use positive decimal native identifiers, not list indices. The final component is zero because raw ScreenCaptureKit IDs have no native-window registry token; it is not a process-ownership flag. |
| `name` | Display label or actual window title. A single display is named `Entire Screen`; multiple displays use `Screen N` labels. |
| `display_id` | Native display ID string for screen sources; empty string for windows. Screen IDs match the macOS display identity used by [screen](screen.md). |
| `thumbnail` | A genuine empty [NativeImage](native-image.md), with no pixel backing. |
| `appIcon` | Null. Window icon fetching is unavailable. |

The provider requests on-screen windows and excludes desktop windows. Results
are fresh snapshots, filtered by the requested types. Names and ordering can
change, and an OS identifier can stop referring to a live source. Returned IDs
are metadata, not permission grants or renderer media handles.

Firefly does not yet connect these IDs to `navigator.mediaDevices.getDisplayMedia`,
`getUserMedia`, or `session.setDisplayMediaRequestHandler`. Enumerating metadata
does not establish a screen-sharing implementation.

## Permissions and platform behavior

The implemented macOS provider requires macOS 12.3 or later and uses
ScreenCaptureKit's asynchronous shareable-content query. A valid enumeration may
request OS permission. It creates no screenshot or stream and reads no image
pixels. Permission belongs to the responsible application/launcher according to
macOS rules; no success is inferred from a build or a preflight check.

There is no public permission-request or permission-bypass helper in this API.
The separate native test calls only a no-prompt permission check and reads active
display identities. It deliberately does not enumerate shareable content, even
when preflight reports access.

Linux and Windows currently return `ERR_FIREFLY_SCREEN_CAPTURE_UNAVAILABLE`.
Their JavaScript policy behavior can be tested independently of OS support;
that does not establish native desktop enumeration on those platforms.

## Bounds, cancellation and errors

There are at most eight pending JavaScript requests. Each has a 30-second deadline.
Successful completion retires the request before resolving its Promise. App exit
rejects pending callers and removes their delivery authority; late native results
are ignored. No public AbortSignal/cancel option is implemented.

The native macOS provider also permits at most eight actual OS operations in
flight. A JavaScript timeout or cancellation **does not release that provider
slot**: it remains occupied until ScreenCaptureKit invokes the real completion.
Repeated timed-out requests cannot create an unbounded number of OS operations.

Snapshots are bounded to 1,024 sources, 4,096 UTF-8 bytes per name, and 1 MiB total
ID/name/display metadata. Duplicate, malformed or inconsistent identities are
rejected rather than replaced by generated sources. Native requests also use
bounded, monotonically increasing request IDs.

| Error code | Meaning |
| --- | --- |
| `ERR_FIREFLY_NOT_READY` | A valid request preceded app readiness. |
| `ERR_FIREFLY_APP_EXITED` | The application exited or the dispatcher stopped. |
| `ERR_FIREFLY_UNSUPPORTED_API` | Thumbnails, window icons or another unsupported option were requested. |
| `ERR_OUT_OF_RANGE` | Too many requested type entries. |
| `ERR_FIREFLY_CAPTURE_LIMIT` | JavaScript/dispatcher pending-request or request-ID limit. |
| `ERR_FIREFLY_CAPTURE_TIMEOUT` | The JavaScript request exceeded 30 seconds. |
| `ERR_FIREFLY_CAPTURE_DATA` | Invalid or oversized native metadata at the JavaScript boundary. |
| `ERR_FIREFLY_SCREEN_CAPTURE_PERMISSION` | ScreenCaptureKit reported that access was declined. |
| `ERR_FIREFLY_SCREEN_CAPTURE_UNAVAILABLE` | Unsupported platform/OS version or missing required entitlements. |
| `ERR_FIREFLY_DESKTOP_CAPTURE_LIMIT` | Native in-flight or metadata budget exceeded. |
| `ERR_FIREFLY_DESKTOP_CAPTURE` | Another native enumeration failure. |

## Validation evidence and remaining work

- [Policy fixture](../../firefly/tests/desktop-capture/policy.mjs): actual Node/Mite
  execute the production API factory with controlled metadata, including option
  snapshots/getter reentry, source validation, filtering, empty-image methods,
  quotas, controlled timeout, cancellation and shutdown. This is a policy unit test.
- [Native dispatcher fixture](../../firefly/tests/desktop-capture/HostProbe.cpp):
  the production `Host.cpp` runs with a controlled native worker completion peer,
  exercising ownership, delivery, cancellation and teardown. It does not call the
  OS provider.
- [Provider preflight fixture](../../firefly/tests/desktop-capture/ProviderProbe.cpp):
  actual no-prompt permission/display checks; no shareable-source enumeration or
  capture. Unsupported providers fail explicitly.
- Actual Electron 45.2.3 comparison covered malformed-option rejection and empty
  image values only. No valid Electron capture-source request was made.

The [opt-in native script](../../firefly/tests/desktop-capture/native-opt-in.mjs)
checks actual screen-source IDs against the native `screen` API. It is deliberately
excluded from unattended gates and has **not been run** for this slice. Running
it may request macOS Screen Recording permission; it captures no pixels.

Valid ScreenCaptureKit enumeration and permission flows need separate authorized
acceptance. Nonempty thumbnails/icons, screenshots, media-stream selection and
native Linux/Windows providers remain planned.
