# ADR 0196: Bounded renderer IPC admission FIFO

Status: native build passed; actual startup evidence recorded in PROGRESS.

## Demand

Code-OSS creates a burst of renderer RPC subscriptions. The old helper refuses
its seventeenth pending send before serialization, even when native transport
would make progress after the current JavaScript turn. Code-OSS catches synchronous
send errors as shutdown, leaving subscriptions absent. Increasing all native
in-flight limits would obscure the distinction between waiting and active work.

## Decision

Keep 16 active calls per document and 64 per renderer. Admit up to 256 additional
waiting calls per document / 1024 per renderer. Both send and invoke use the same
FIFO: a later invoke must not publish a higher native request ID before an earlier
queued send. The total retained record maxima are therefore 272 and 1088.

Capture immutable wire snapshots at call time and charge them to existing shared
16 MiB document / 64 MiB process wire budgets. Transfer that reservation into the
physical queue on promotion, without another copy or reservation. Publish the
private native control marker and matching wire only when active credit and the
physical queue have capacity. Keep physical framing and its separate 48-entry /
16-write limits. One promotion per deferred turn allows other documents to use
renderer-wide credit; settlement wakes weakly registered waiting endpoints.

Use C++ because native endpoint ownership, byte-channel queues, GC capabilities,
deadlines and document revocation already live there. No new JavaScript scheduler,
upstream modification or Mite-to-Firefly dependency is introduced.

The 30-second deadline begins at the original call and never restarts. Expire an
unsent call individually. An admitted timeout retires the channel, since main may
still hold its active slot or a partial fragment; helper credit cannot safely be
reused in isolation. A received reply already deferred for safe VM entry wins over
timeout. Navigation/teardown releases queued reservations and active accounting.
Unlink weak endpoint registration before destructor revocation/wake.

## Evidence and limits

Strict compilation and the native Release host/helper/offscreen rebuild pass.
The existing overflow fixture now fills the waiting bound instead of treating the
seventeenth call as exhaustion; fresh local execution is deferred to prioritize the
actual Code-OSS entry. Hosted CI remains responsible for the broader IPC gates.
Full queues and retained-wire exhaustion still refuse explicitly. This does not
promise unlimited Electron queuing, delivery after termination, editing readiness
or acceptance on another platform.
