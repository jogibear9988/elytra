# ADR 0207: Descriptor-specific native standard output

Status: implemented; focused local macOS runtime acceptance, 2026-11-17.

## Problem

Global Console must use the real process.stdout/stderr Writable streams, including
application write hooks and shared FIFO/error policy. Their previous transport
always queued filesystem worker requests. Even small messages could be lost on
immediate exit, keep unrelated unreferenced immediates alive, or wait behind four
blocking filesystem operations before a DNS responsiveness test could print.
Linux and macOS CI expose these failures; reverting to raw Console writers would
break the shared stream contract rather than correct its transport.

## Decision

Classify borrowed descriptors 1 and 2 natively. Files write synchronously on all
platforms, as do POSIX terminals and Windows pipes. POSIX pipes/sockets use libuv
stream writes: try the kernel immediately, then copy and retain only the unwritten
suffix in an actual asynchronous request. Windows terminals use their native
libuv stream. Windows TCP-backed stdio explicitly rejects unsupported CRT/socket
handle conversion. No output write is submitted to the filesystem worker pool. An
immediately completed write creates no native pending work; an actual uv_write
request retains loop liveness until completion. Writable preserves callback
scheduling, serialization, backpressure and stream hooks.

Native copies are bounded to 2 MiB per chunk and 8 MiB retained pending bytes per
runtime; excess writes fail with ENOBUFS. Errors carry native status/code and the
write syscall. Ending/destroying the JS wrapper does not close descriptor 1/2 or
call shutdown on the underlying pipe/socket. Runtime teardown closes owned
duplicates and cancels outstanding callbacks without entering application code.

## Borrowed ownership

Duplicating a POSIX descriptor does not isolate its O_NONBLOCK status. Snapshot
both outputs before either native stream opens: stdout/stderr may alias the same
open file description. Concurrent runtimes share the same process-wide lease and
owned CLOEXEC anchors. Restore only the original nonblocking bits through those
anchors after the final runtime, native request and handle close; preserve other
status flags. Abrupt process-ending paths restore flags without closing anchors,
flushing output or running callbacks. Signal-caused termination or crashes cannot promise
cleanup. Live asynchronous streams necessarily share nonblocking state with their
host. Hosts must keep their standard descriptors stable while runtimes are active;
replacing fd 1/2 during that interval is outside this contract.

## Acceptance boundary

Release standalone Mite/embedded host and Firefly/offscreen builds pass. Actual
Node/Mite controls pass raw/UTF-8 output, factory short-write behavior, small-write
unref behavior, regular files, a slow independent >2 MiB pipe drain with byte-exact
suffix/FIFO/backpressure, real closed-pipe EPIPE error/callback identity, and
separate/aliased borrowed descriptor restoration. The genuine embedded host also
passes the restoration checks. Six targeted exit/immediate/abort/process-error/
DNS/performance runners pass, as do Console and builtin controls. Logs:
build/native-stdio-{controls,regressions,console-controls,builtin-controls}.log.
Native Linux CI at fb655df subsequently passes all 161 baseline suites, including
the CLI stdio controls. Windows and direct TTY runtime acceptance remain open;
this does not accept later runtime changes or Linux renderer packaging. The original Code-OSS editor/terminal workflow passes after this
rebuild; all fifteen observed processes stop, with two helper cleanup signals.
That run does not prove fully natural helper teardown. Explicit process.exit may abandon truly pending asynchronous
output; no arbitrary event-loop drain is permitted after terminal exit dispatch.
process.stdin, public TTY/ref-unref features and Node's special non-ending stdio
wrapper behavior remain separate work.

References: [Node 24.15 process I/O](https://nodejs.org/download/release/v24.15.0/docs/api/process.html#a-note-on-process-io),
[Node descriptor selection](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/bootstrap/switches/is_main_thread.js),
[libuv 1.52.1 stream writes](https://github.com/libuv/libuv/blob/v1.52.1/src/unix/stream.c).
