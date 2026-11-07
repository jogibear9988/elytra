# protocol

Status: partial main-process API. registerSchemesAsPrivileged publishes the native
origin table; resource handlers, sessions and automatic window grants remain
unimplemented. No general Electron protocol compatibility is claimed.

## Import

```js
import {protocol} from 'firefly';

protocol.registerSchemesAsPrivileged([
  {scheme: 'app', privileges: {standard: true, secure: true}}
]);
```

Call during main-module initialization, before app readiness and before any
WebContent helper starts. This configures origin identity, not a URL-to-file route.
It does not make window.loadURL('app://bundle/index.html') supported yet.
The explicit --electron-compat alias and CommonJS expose the same main singleton.
Preload and renderer exports do not include protocol. Standalone Mite has no
Firefly origin policy dependency.

## protocol.registerSchemesAsPrivileged(customSchemes)

Returns undefined. Accepts an array of at most 32 objects with scheme and optional
privileges. Names must use canonical lowercase ASCII RFC scheme spelling, start
with a letter, and contain at most 64 characters. Built-in/internal names and
duplicates are refused. Unknown options/privileges and nonboolean privilege values
are errors. Omitting privileges uses false defaults.

| Privilege | Current contract |
| --- | --- |
| standard | Native/Rust tuple origin for hierarchical URLs with a valid nonempty host; preserve custom scheme and explicit port |
| secure | Mark a standard custom tuple origin as trustworthy; requires standard: true |
| bypassCSP, allowServiceWorkers, supportFetchAPI, corsEnabled, stream, codeCache | true fails with ERR_FIREFLY_UNSUPPORTED_API; false requests no capability |

Secure nonstandard origins are explicitly unsupported. Registration supplies no
CSP bypass, CORS permissions, service workers, fetch handler, media policy or V8
cache. Existing HTTP/HTTPS/file behavior remains upstream. Actual native resource
loading has separate internal acceptance, documented in
[ADR 0123](../architecture/0123-live-document-resource-handoff.md).

Validate the whole batch before publishing. Invalid/unsupported input does not
consume the registration or partially install earlier entries. One successful
call, including an empty array, permanently consumes registration. Native code
independently validates the bounded JSON schema and atomically publishes an owned,
immutable process table. Mutating input afterward cannot change it. Starting even
an unconfigured WebContent helper seals registration to avoid inconsistent helpers.
There is no reset/unregister operation for origin privileges.

| Failure | Error code |
| --- | --- |
| Incorrect argument/property type | ERR_INVALID_ARG_TYPE |
| Invalid/reserved/duplicate name or unknown option | ERR_INVALID_ARG_VALUE |
| More than 32 entries or native wire budget exceeded | ERR_OUT_OF_RANGE |
| Requested unsupported privilege | ERR_FIREFLY_UNSUPPORTED_API |
| Call after app readiness | ERR_FIREFLY_PROTOCOL_TOO_LATE |
| Repeated successful registration or helper launch already started | ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED |

## Remaining API

handle, unhandle, isProtocolHandled, registerFileProtocol and other handler APIs
are unavailable. session/defaultSession/partitions, navigation ownership and
main-side asynchronous file authorization still need product integration. The
normal runtime grants no document resource access based on this registration.

## Validation

check-firefly-protocol-registration runs maintained public ESM against the actual
LibJS/native binding and native/Rust URL table, without Qt or LibWeb. It checks
atomic validation/refusal, native schema validation, owned helper snapshots,
standard/secure/opaque origins, repeat calls, readiness and post-launch sealing.
check-firefly-protocol-host checks the actual Firefly main export/registration/
readiness contract without constructing a window. Local macOS Release passes;
fresh hosted platform execution remains required.

```sh
cmake --build build/renderer --target check-firefly-protocol-registration check-firefly-protocol-host --parallel 3
```

Baseline: [Electron protocol](https://www.electronjs.org/docs/latest/api/protocol).
Only the subset described above is implemented. Pinned Code-OSS asks for additional
privileges and APIs; this export does not imply those requests are accepted.
