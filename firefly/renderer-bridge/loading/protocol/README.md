# Native LibWeb protocol response adapter

Response consumes the bounded native file frames and builds actual LibWeb pending
responses/body channels. It owns the fetch controller/params until a terminal
path, uses a weak transport proxy, and forwards native credit/cancel notifications.
Dispatch adds a native-only scoped provider slot to the owned scheme-fetch seam.
It defaults to empty; product/diagnostic helpers install no provider. Only registered
standard schemes, same-origin top-level Window requests and GET/HEAD are admitted;
Range and worker requests are denied. Navigation requires a separate explicit
native handler and one-shot intent; it stays denied for existing document providers.
Upstream fetch policy still runs.
No JavaScript registration capability or public protocol handler is installed here.

The class compiles inside LibWeb because upstream PendingResponse is intentionally
not exported. The header declares an owned native API; this is not a new Electron
module. LibWeb links only firefly-protocol-file-codec, not the file/request owner.

```sh
cmake --build build/renderer --target check-firefly-protocol-fetch --parallel 3
```

The gate reads actual files into real LibWeb ReadableStream consumers, with GC,
bounded credit, real controller/reader cancellation and malformed-input checks.
It needs no desktop window. Only local macOS component execution is accepted so
far; hosted Linux/Windows evidence must remain separate.

See [ADR 0116](../../../../docs/architecture/0116-libweb-protocol-responses.md) for
ownership, cancellation, validation and remaining renderer integration.

The gate also invokes actual JavaScript fetch/arrayBuffer and AbortController in
a principal realm/document with a native same-process file provider. It checks
forbidden header filtering, scope denial, unchanged data: fetching, revocation and
registration lifetime. This is not live WebContent IPC or public protocol support.
See [ADR 0117](../../../../docs/architecture/0117-native-scheme-fetch-dispatch.md).

Client now connects Response to the native main-side FileServer over bounded byte
IPC. It roots one exact active document, checks local native settings identity in
addition to origin, and releases VM ownership on cancellation/revocation/destruction.
It performs no file IO and compiles separately from LibWeb. Creating it installs
neither a provider nor a page API; product helpers still create none.

```sh
cmake --build build/renderer --target check-firefly-protocol-fetch-channel --parallel 3
```

This gate is also required by check-firefly-protocol-fetch. It executes real global
fetch in a separate native LibWeb child; the parent alone opens the file. It tests
real body/HEAD/refusal, quotas, foreign same-origin documents, abort/reader cancel,
owner destruction, peer loss, malformed responses and a real silent-peer deadline.
This is not live WebContent bootstrap/document routing. See
[ADR 0119](../../../../docs/architecture/0119-document-fetch-channel-client.md).

Navigation provides the separate one-shot browser-UI scope used by
Client::create_navigation. It checks the current native ID/navigable/source settings
and expected URL against the actual upstream-created navigation request. It does
not make a document resource client accept navigation or install a helper service.

```sh
cmake --build build/renderer --target check-firefly-protocol-navigation --parallel 3
```

The real upstream navigation population gate is required by the fetch gate. It
checks descriptor body bytes/origin/CSP, exact 16-MiB acceptance/over-limit refusal,
foreign native navigables, one-shot/default/page-source refusal and body teardown.
Upstream fully buffers these initial bodies without a RequestServer transfer lease;
Response rejects oversized navigation metadata before creating the body channel.
Normal resource response limits remain unchanged. The test leaves the source
about:blank document untouched; actual document commit/live helper routing remain
open. See [ADR 0121](../../../../docs/architecture/0121-native-navigation-population.md).
