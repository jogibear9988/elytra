# ADR 0090: Native process clocks and CPU samples

Status: partial API expansion; macOS native/Node/GC/npm acceptance, hosted new
platform acceptance pending.

## Decision

Keep timing policy in an owned ESM module beside the process initializer, and
native measurements in platform/process/Timing.cpp. Expose only fresh primitive
samples across the private host boundary. No timer/worker handle or JS value is
retained. Mite remains independent of Qt, Firefly, LibWeb, Node and V8.

Use uv_hrtime for monotonic nanoseconds. Split the native u64 into seconds and
remainder before conversion to JS Number, preserving every bit when assembling
the public BigInt. Keep ordinary legacy tuple subtraction/borrow separately.
Use uv_getrusage for process user/system microseconds, with ordinary native error
metadata. These are CPU counters, not JS task elapsed time.

Initialize a steady-clock uptime origin once at runtime-library load. Avoid
calling libuv during static initialization before platform setup, and avoid
per-realm resets. Dynamic embedders observe runtime-library lifetime rather than
time before loading Mite; document that distinction.

## Acceptance boundaries

Compare actual Node/Mite global/CJS/ESM identities, native clock sandwich samples,
elapsed timer advancement, wall-clock independence, CPU work/deltas, validation
and getter behavior. Force collection in actual Node and the non-GUI embedded
host. Separate deterministic owned-policy tests cover u64 maximum, borrow,
fractions and native-error propagation without pretending to be OS probes.

Exercise published time-span 5.1.0 and convert-hrtime 5.0.0 with the committed npm
lock and preserved upstream licenses. A counting wrapper delegates every sample
to the original native hrtime.bigint; no clock result or package source is faked.
Full getter/proxy/reentrant compatibility, provider failure injection, physical
long-uptime/embedding timing and further hosted/platform profiles remain open.
