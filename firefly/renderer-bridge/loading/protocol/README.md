# Native LibWeb protocol response adapter

Response consumes the bounded native file frames and builds actual LibWeb pending
responses/body channels. It owns the fetch controller/params until a terminal
path, uses a weak transport proxy, and forwards native credit/cancel notifications.
No JavaScript capability, scheme registration or URL handler is installed here.

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
