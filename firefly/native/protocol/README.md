# Native custom-protocol foundation

OriginPolicy supplies native custom origin identity and LibWebCommon's matching
potentially-trustworthy decision. OriginConfig validates and delivers a declared
parent snapshot before the owned renderer helper bootstrap. This is internal
infrastructure, not an exported protocol object or a custom resource handler.

The policy starts empty. Native code may install one immutable bounded table
before the first WebContent launch. Launching even an unconfigured helper seals
the empty policy, without adding a private option, and rejects later registration.
This prevents mismatched prelaunched/new helpers. No JavaScript, page, preload or
environment API configures the table.

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

The actual DOM URL class uses Rust directly and currently bypasses this C++ origin
policy: new URL('app://bundle/').origin remains null in the renderer. The pinned
Rust SchemeType/parser has no custom-standard extension. Treat this as an open
engine integration blocker, not standard-scheme compatibility. The public protocol
export remains absent and Code-OSS still fails at that import.

Targets.cmake composes three fingerprinted translation units, preserving upstream
bytes outside owned include/call seams and all notices. Drift or changed target
ownership fails configure/build. Ladybird stays untouched; standalone Mite does
not link the policy. Its bounded immutable tables live until process exit.

```sh
# Actual native origins/trust, codec, argv lifetime and launch sealing; no GUI.
cmake --build build/renderer --target check-firefly-protocol-origins --parallel 3
# Actual native parent/diagnostic/product helper delivery; no desktop windows.
cmake --build build/renderer --target check-firefly-protocol-renderer --parallel 3
# Separate currently failing DOM URL origin acceptance, not a supported API gate.
cmake --build build/renderer --target check-firefly-protocol-dom-origins --parallel 3
```

The native tests verify default denial, real tuple/trust results, scheme/host/port
separation, IDNA/IPv6, atomic validation, concurrent reads, serialization, strict
options, temporary-input ownership and late-registration refusal. The headless
tests verify actual child PID/policy before bootstrap, first-script observation,
default denial and absent page privileges; they report the DOM URL gap as PENDING.
Native macOS passes. Linux/Windows need separate results; the Windows renderer
has no owned helper delivery or production desktop acceptance yet.

Remaining work before public registration:

1. Consistent Rust DOM/native origin extraction and full custom-standard URL
   parsing/mutation/serialization, without a private upstream parser fork.
2. Main-process registration lifecycle before helper/spare launch; worker-process
   policy propagation and platform acceptance.
3. Actual owned request/file-response transport with bounded resources,
   authorization, cancellation and native lifetime.
4. LibWeb fetch/navigation/CORS/MIME/CSP integration, then required window/helper
   acceptance and actual Code-OSS workbench resources.

See [ADR 0112](../../../docs/architecture/0112-native-protocol-configuration.md).
