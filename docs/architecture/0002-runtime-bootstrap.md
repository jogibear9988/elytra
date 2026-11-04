# ADR 0002: first runtime host

Status: accepted; first CLI slice built and tested on macOS ARM64.

The CLI owns one LibJS VM and host realm. It evaluates a script with `Script::parse`
and `VM::run`; it does not launch the upstream `js` executable or another runtime.
The first host deliberately implements a narrow API and publishes the limitations.
Node's CLI, global and process documentation plus the differential fixtures are the
behavioral reference.

Promise jobs are queued as rooted GC functions. Their captures keep both the job
and realm alive. Jobs added while draining run FIFO in the same checkpoint.
Rejections stay rooted until handled or reported after the checkpoint. Native
callbacks must never retain a bare JS heap pointer across an allocation/checkpoint.

The event loop and validated timers are specified in ADR 0003. A pending promise
alone does not keep the runtime alive. Future promise-returning module evaluation
must detect unsettled top-level await rather than report success.

The bootstrap is a script host, not a CommonJS implementation. A separate module
loader must supply Node package resolution and CJS wrapper semantics. Native
services belong in host modules; renderer APIs must never enter the Mite target.
