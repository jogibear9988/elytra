# ADR 0015: a separate nextTick FIFO

Status: implemented subset, tested on macOS ARM64 against Node v24.15.0.

Add a rooted nextTick FIFO to EventLoop rather than routing nextTick through the
Promise queue. The bootstrap captures the native enqueue hook and Reflect.apply,
installs process.nextTick with argument/callback validation and removes the hook.
A queued JS closure retains its callback and arguments through the rooted native
GC function; callbacks receive undefined as their explicit receiver.

At a checkpoint, drain ticks, then the existing microtask FIFO, repeating while
new ticks exist. Never interrupt a currently draining microtask queue. This
preserves Node's CJS versus ESM startup distinction: CJS application code runs
before the initial checkpoint, whereas our ESM evaluation happens in Promise
jobs. Run rejection checks only after both queues reach a fixed point. A tick
may therefore attach a rejection handler before unhandled-rejection reporting.
Ticks can schedule native work and keep JS computation pending without a timer.
Exceptions stop further dispatch, using the existing fatal-I/O termination policy.

Fifteen differential fixtures cover both entry formats, nested tick/microtask
queues, optimized await continuation, arguments/receiver, invalid callbacks,
rejection handling, module identity, timer/filesystem callback checkpoints and
thrown ticks. Stdout/status and error markers are compared, not stack formatting.
The engine's Promise-queue-empty hook remains specific to microtasks: pending
ticks do not preempt an await continuation already inside that drain.

This is not the full Node loop. Immediates, process rejection/lifecycle events,
warning scheduling, async_hooks and phase-specific native networking behavior
remain pending. A recursive tick can starve I/O as it can in Node; no invented
fairness cap changes that ordering. Ladybird remains unmodified.

Reference: [Node nextTick](https://nodejs.org/docs/latest-v24.x/api/process.html#processnexttickcallback-args).
