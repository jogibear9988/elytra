# 0065: Immediate TCP write completion without synchronous public callbacks

Status: implemented; local runtime acceptance passes, new hosted acceptance pending.

## Evidence and decision

Windows ef61124 run 42147228418 / job 127661590822 failed the HTTP client
comparison because its first GET response finished consumption before the request
finish listener ran. The recorded finished field is a listener observation, not
the writableFinished getter. Mite queued every TCP write through uv_write,
including an empty write after the request headers, while Node's transport first
attempts immediate completion. An independent local workflow exposes the difference:
after small/empty/small writes, Node's writableLength is [0,0,0], versus Mite's
previous [1,1,2]. Both deliver ab and asynchronously invoke all three callbacks.

Add uv_try_write in the owned native TCP adapter. Return a private synchronous
success marker when all bytes are accepted; the JS Socket passes that result to
the existing Writable callback mechanism, which defers public callbacks. Never
enter application JS from the native write call itself. Empty writes with no
native queued bytes complete through this same path without adding an OS request.
Partial immediate writes queue only the unsent suffix. EAGAIN/ENOSYS use the
existing asynchronous path; other native errors retain their status. Queued
requests own the byte allocation and rooted callback through completion/cancel.
No new dependency, blocked I/O mode, upstream patch or stream-source edit is used.

Follow the [libuv stream contract](https://docs.libuv.org/en/v1.x/stream.html) and
Node's [native write path](https://github.com/nodejs/node/blob/v24.15.0/src/stream_base.cc).
This is a transport correction, not a forced ordering of arbitrary HTTP replies.

## Validation and limits

The new shared TCP workflow compares empty/immediate writable lengths, exact
payload/acknowledgement and callback order/asynchrony. Existing independent OS
peer, direct/corked 3 MiB transfers, byte checksums, cancellation, fatal callback
and shutdown tests remain enabled. The HTTP differential suite records the finish
listener plus writableFinished/writableEnded, repeats 24 immediate complete-request
round trips, and checks an independent peer's early 413 before the request upload
has ended: that response must not fabricate request completion or wait for it.

Run all runtime suites and the actual Firefly networking workflow after rebuilding
the shared host runtime. Record their results in PROGRESS. OS acceptance into a
send buffer is not peer receipt, and arbitrary server responses can arrive before
an upload finishes. Native Windows/Linux acceptance and any new package size are
separate gates. TLS, agents and full Node stream/event-order parity remain open.
