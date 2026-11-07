# Native custom-protocol foundation

OriginPolicy supplies process-local custom origin identity and the corresponding
LibWebCommon potentially-trustworthy decision. It is internal infrastructure,
not an exported protocol object or a working custom resource handler.

The policy starts empty in every product/diagnostic helper. Only native code can
install one immutable bounded table. No JavaScript, preload, environment variable
or user-supplied helper argument currently configures it. The native probe installs
a test table in its own process and exercises actual composed LibURL/LibWebCommon.

For standard entries, hierarchical nonempty valid domain/IP hosts produce tuple
origins with the custom scheme and original explicit port. The existing host
parser supplies canonical domain/IDNA/IP identity. Unknown/nonstandard/invalid/
opaque forms retain upstream opaque origins. Secure entries additionally require
standard tuple origins; the secure flag alone cannot bless an opaque origin.
Unmodified HTTP/HTTPS/loopback/file trust remains on upstream paths. No CSP, CORS,
fetch, ServiceWorker or V8 cache privilege is granted by this component.

Targets.cmake composes two fingerprinted translation units; all original bytes
outside the owned include and early-return seams are preserved. Source drift or
changed CMake ownership fails configuration/build until reviewed. Ladybird remains
untouched; the standalone Mite graph does not include this policy. The table lives
until process exit, bounded to 32 entries / 64-byte canonical lowercase names.

Run the non-GUI native component gate:

```sh
cmake --build build/renderer --target check-firefly-protocol-origins --parallel 3
```

This verifies default denial, actual origin/trust results, host/scheme/port
separation, IDNA/IPv6, failed-batch atomicity, immutable registration and concurrent
reads. Native Linux/Windows results are required separately from macOS acceptance.

To complete the Electron protocol path, implement and verify:

1. Full custom-standard URL parsing/mutation/serialization semantics; this origin
   hook does not make Rust's non-special parser behave like Chromium's registered
   standard parser. Keep opaque-host parsing differences visible until resolved.
2. Validated immutable main/helper configuration before application documents,
   rejecting unsupported privileges rather than silently dropping them.
3. Actual request/response and file-handler transport with owned lifetime, bounded
   resources, cancellation, authorization, and default-deny routing.
4. LibWeb fetch/navigation/CORS/MIME/CSP integration and native window/helper tests.
5. Only then export the tested protocol subset and use actual Code-OSS startup,
   workbench resources and adversarial controls as acceptance.

Code-OSS still fails at the absent protocol import; this component is not a desktop
launch. See [ADR 0111](../../../docs/architecture/0111-custom-protocol-origins.md).
