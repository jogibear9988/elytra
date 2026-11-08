# Local stream endpoints for Code-OSS

The next actual Code-OSS startup dependency after the explicit native policy
adapter is `ipc.net.serve(path)` in the single-instance claim. This requires a
real local OS endpoint: a Unix-domain stream socket on macOS/Linux and a named
pipe on Windows. Mapping these paths to TCP would change permissions, naming,
conflict handling and cleanup semantics.

## Decision

Extend Mite's existing libuv network owner with a transport kind and use
`uv_pipe_t` with `ipc = 0`. These are byte streams; this does not implement
descriptor passing or Firefly's renderer/utility-process port handoff. Keep
LibJS integration in C++ and public overload/stream policy in maintained ESM.
No new engine, runtime dependency or Ladybird modification is needed.

Use length-aware `uv_pipe_bind2` and `uv_pipe_connect2` with
`UV_PIPE_NO_TRUNCATE`. Preserve native endpoint errors. A failed bind must never
delete the existing socket or a regular file. In Code-OSS, the application owns
the explicit stale-endpoint `ECONNREFUSED` unlink/retry policy. Native socket
cleanup remains libuv's responsibility for endpoints it successfully owns.

Pipe handles share the TCP stream's pending byte ownership, half-close, flow
control, cancellation, liveness and close callbacks. Accepted handles inherit
the listening transport kind. Pipe addresses must not be cast to TCP structures;
`Server.address()` returns the original path and `Socket.address()` returns an
empty object. TCP-only keep-alive and no-delay settings are harmless on pipes.

## Acceptance boundary

Use Node differential fixtures and an independent OS socket peer. Run the
unchanged pinned Code-OSS IPC consumer in separate processes, exchanging genuine
VSBuffer channel requests/replies in both Node/Mite directions. Verify compiled
dependency provenance before execution. A local IPC component pass does not
prove a workbench launch, editing, extension-host operation, descriptor passing
or native Windows execution. Record actual results and remaining limits in
[the progress log](../PROGRESS.md) and [the API contract](../api/net.md).

References: [libuv pipe API](https://docs.libuv.org/en/v1.x/pipe.html),
[Node net API](https://nodejs.org/api/net.html).
