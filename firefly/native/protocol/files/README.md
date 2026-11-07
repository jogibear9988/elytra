# Native regular-file response bodies

This is the main-side body component for future protocol file responses. It is
not an exported protocol handler or URL-to-path resolver. Trusted native code must
already have authorized the path returned by the application's handler. No renderer
path, root, request or descriptor is admitted by this component itself.

`FileBody::open` takes an absolute path, checks the opened descriptor is a regular
file, and captures its size. Paths are bounded to 32,768 bytes and reject NUL;
files larger than 64 GiB fail explicitly. POSIX opens nonblocking before checking
the descriptor so a FIFO cannot stall admission. Windows checks for a disk handle
and a regular file. Symlinks may resolve to a regular file: authorization, root
containment and symlink policy remain the caller's responsibility.

One owned descriptor serves all reads; chunks never reopen the path. A caller
pulls at most 64 KiB per read and controls scheduling/backpressure. Empty reads
mean complete body delivery. HEAD preserves content length without a body. EOF,
HEAD, cancellation, errors and destruction close the descriptor. Cancellation is
idempotent and subsequent reads fail. Premature EOF after file truncation is an
error; growth cannot exceed the originally captured length. This is a bounded live
file reader, not an immutable content snapshot. Concurrent edits within that length
can change bytes. Reads are synchronous; asynchronous IO/worker scheduling belongs
to the future request service.

```sh
cmake --build build/renderer --target check-firefly-protocol-files --parallel 3
```

The native gate reads a real 4 MiB + 19 byte file and streams 65 bounded chunks over
the existing byte channel to a separate native child. The child independently
checks all bytes and returns its kernel PID and cumulative position. It never
receives the source path. The probe also checks empty files, HEAD, chunk budgets,
cancellation, growth, descriptor identity across path replacement, truncation,
missing/relative/NUL paths and directory refusal. POSIX adds FIFO/device/oversize
refusal. Only local macOS execution is accepted so far; native Linux/Windows CI
must supply their own results.

Still required before Code-OSS resources work: versioned request/metadata framing,
trusted helper/document admission, bounded concurrent request lifecycle and
cancellation on navigation/crash, application handler dispatch, MIME and response
headers, renderer fetch/navigation integration, sessions and the public Electron
protocol APIs. This component does not enable `file:` access from a custom origin
or change existing renderer file permissions.
