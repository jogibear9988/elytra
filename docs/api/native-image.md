# nativeImage

Status: **empty-image subset**. `nativeImage.createEmpty()` returns a real
`NativeImage` instance with no pixel backing. Its supported empty-value methods
pass Node/Mite policy tests, and their values were compared with Electron 45.2.3.
Nonempty image loading, encoding, thumbnails and native handles are unavailable.
This does not establish general Electron image compatibility.

The object is a Firefly main-process export, including through the explicit
Electron compatibility alias. It does not add image or filesystem capabilities
to ordinary renderer scripts or standalone Mite.

```js
import { nativeImage } from 'firefly';

const empty = nativeImage.createEmpty();
console.log(empty.isEmpty()); // true
console.log(empty.getSize()); // { width: 0, height: 0 }
console.log(empty.toPNG().length); // 0
```

Creating an empty value does not require app readiness, open a file, enumerate
the desktop, request permission, or read pixels.

## Supported factory and methods

`nativeImage.createEmpty()` returns a new empty instance. There is no public
`NativeImage` constructor; direct construction through an instance's constructor
rejects with `ERR_ILLEGAL_CONSTRUCTOR`. Methods require a genuine instance.

| Method | Empty-value result |
| --- | --- |
| `isEmpty()` | True. |
| `getSize()` | Fresh `{ width: 0, height: 0 }`. |
| `getAspectRatio()` | 1, matching the measured Electron empty-image result. |
| `getScaleFactors()` | Fresh empty array. |
| `toPNG()` | Zero-length Buffer. |
| `toJPEG(quality)` | Zero-length Buffer; no JPEG encoding occurs. |
| `toBitmap()` / `getBitmap()` | Zero-length Buffer; no raw pixel data exists. |
| `toDataURL()` | `data:image/png;base64,`. |
| `crop(rect)` / `resize(options)` | Another empty NativeImage. No pixel geometry is transformed. |
| `isTemplateImage()` | False. |
| `setTemplateImage(value)` | Returns undefined; an empty image remains non-template. |

The zero-length buffers are not usable PNG/JPEG files. The data URL contains no
encoded image. These methods describe an empty value, including the metadata-only
[desktopCapturer](desktop-capturer.md) thumbnails; they do not fabricate previews.
Additional Electron argument-validation and platform-image behavior are not
accepted by this subset.

## Explicitly unavailable

The following factories reject with `ERR_FIREFLY_UNSUPPORTED_API`:

- `createFromPath`
- `createFromBuffer`
- `createFromDataURL`
- `createFromBitmap`
- `createFromNamedImage`
- `createThumbnailFromPath`

`createThumbnailFromPath` returns a rejected Promise; the other listed factories
throw synchronously. They remain unavailable even when a particular input might
produce an empty image in Electron. `image.addRepresentation()` and
`image.getNativeHandle()` also fail explicitly.

Image import/decoding, nonempty representations, PNG/JPEG encoding, filesystem
thumbnail generation, icons, native handles and full renderer/IPC image transport
remain planned.

## Validation

The [desktop-capture policy fixture](../../firefly/tests/desktop-capture/policy.mjs)
executes the production image factory in actual Node and Mite with controlled
source metadata. It checks empty dimensions, byte buffers, data URL, transforms,
template behavior. A separate actual Electron 45.2.3
empty-image probe measured the corresponding values without windows, capture
requests or OS permission prompts.

No nonempty image codec or OS screenshot acceptance is claimed. See the
[Electron nativeImage reference](https://www.electronjs.org/docs/latest/api/native-image)
for the broader upstream API.
