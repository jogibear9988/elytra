# Native custom-protocol foundation

OriginPolicy supplies native custom origin identity and LibWebCommon's matching
potentially-trustworthy decision. OriginConfig validates and delivers a declared
parent snapshot before the owned renderer helper bootstrap. The main-only
RegistrationHost binding now connects the partial public protocol registration
API to this table. routes/ owns default-session public file handlers and their
trusted native navigation/document grants. General Response handlers remain open.

The policy starts empty. Native code may install one immutable bounded table
before the first WebContent launch. Launching even an unconfigured helper seals
the empty policy, without adding a private option, and rejects later registration.
This prevents mismatched prelaunched/new helpers. Only the trusted main-process
registration API can configure it; pages, preloads and environment variables cannot.

When the native parent has declared a policy, its actual WebContent process launch
adds --firefly-origin-policy=JSON. Only the owned helper entry consumes it, before
upstream main/VM initialization. An unknown ordinary upstream helper fails closed
on that unsupported option; there is no silent fallback. The exact native schema
is version 1, maximum 8 KiB / 32 entries, with scheme/standard/secure keys only.
Unknown privileges and invalid/reserved/duplicate schemes reject atomically.
Filtered argv storage stays owned through upstream entry completion. A configured
helper emits a read-only PID/policy startup diagnostic while this path is under
development. No filesystem configuration or page messaging is involved.

For standard entries, native hierarchical nonempty valid domain/IP hosts produce
tuple origins with the custom scheme and original explicit port. The existing
host parser supplies canonical domain/IDNA/IP identity. Unknown/nonstandard/
invalid/opaque forms retain upstream behavior. Secure entries additionally require
standard tuple origins. Default HTTP/HTTPS/loopback/file trust remains upstream.
No CSP, CORS, fetch, ServiceWorker or code-cache capability is supplied here.

Native and DOM consumers now share the real Rust Url::origin entry, which consults
the canonical C++ policy through an existing borrowed FfiOrigin callback ABI.
The earlier C++ URL::origin override is removed. The DOM getter remains upstream;
other Rust extraction callers receive the same policy. The pinned Rust parser
still has no custom-standard extension: complete parsing/mutation behavior remains
open. Public protocol now also exposes default-session file callbacks; Code-OSS now passes that
named import and fails at missing crashReporter. Real public file routing passes locally; no Code-OSS startup pass.

Targets.cmake composes three fingerprinted C++ translation units for trust, shared
fetch classification and helper launch. The owned Rust build driver selects guarded root/module/origin compositions,
preserving all other source bytes and license notices, original manifests/lockfiles
and parser modules. Changed fingerprints invalidate Cargo artifacts; actual archive
dependency outputs must include the adapter. Drift or changed module/target ownership
fails closed. Ladybird stays untouched; standalone Mite does not link the policy.
Its bounded immutable tables live until process exit. See
[ADR 0113](../../../docs/architecture/0113-shared-rust-origin-policy.md).

```sh
# Actual native origins/trust, codec, argv lifetime and launch sealing; no GUI.
cmake --build build/renderer --target check-firefly-protocol-origins --parallel 3
# Actual native parent/diagnostic/product helper delivery; no desktop windows.
cmake --build build/renderer --target check-firefly-protocol-renderer --parallel 3
# Alias for the required strict renderer policy/origin acceptance.
cmake --build build/renderer --target check-firefly-protocol-dom-origins --parallel 3
```

The native tests verify default denial, real tuple/trust results, scheme/host/port
separation, IDNA/IPv6, atomic validation, concurrent reads, serialization, strict
options, temporary-input ownership and late-registration refusal. The headless
tests verify actual child PID/policy before bootstrap, first-script observation,
default denial, absent page privileges, real getter branding and normalized DOM
origins for credentials, explicit ports, IDNA, IPv6 and relative URLs. The strict
origin checks are required by the renderer configuration gate.
Native macOS and 1d033fe Linux (run 42399004586) origin/helper gates pass. The Windows renderer
has no owned helper delivery or production desktop acceptance yet.

The [native file-body component](files/README.md) now reads checked regular files
in bounded chunks and has a separate-process byte-transfer gate. Versioned heads
carry checked MIME/length and ordered HTTP header bytes before the body. Renderer
requests, application callbacks and applying these responses remain unconnected.

Remaining work before public registration:

1. Full custom-standard URL parsing/mutation/serialization, without a private
   upstream parser fork; shared native/DOM origin extraction is now accepted locally.
2. Main-process registration lifecycle before helper/spare launch; worker-process
   policy propagation and platform acceptance.
3. Actual owned request/file-response transport with bounded resources,
   authorization, cancellation and native lifetime.
4. LibWeb fetch/navigation/CORS/MIME/CSP integration, then required window/helper
   acceptance and actual Code-OSS workbench resources.

See [ADR 0112](../../../docs/architecture/0112-native-protocol-configuration.md).

The [shared fetch-scheme classification](../../../docs/architecture/0120-custom-fetch-scheme-classification.md)
now makes registered standard schemes reach the engine/native UI fetch path.
This grants neither HTTP/local status nor a navigation response handler. Actual
custom navigation loading and live resource bootstrap remain open.

```sh
# Pure native engine/UI routing checks: no window or external application launch.
cmake --build build/renderer --target check-firefly-protocol-routing --parallel 3
```
