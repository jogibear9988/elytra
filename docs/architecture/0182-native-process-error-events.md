# ADR 0182: Native process error and rejection delivery

Status: implementation; validation results are recorded in PROGRESS.

Code-OSS ErrorTelemetry registers uncaughtException and unhandledRejection during
main-service startup. Mite previously refused these events because its native
loop had no delivery path. EventEmitter registration alone would hide the missing
behavior and could discard actual application failures.

The private builtin bootstrap now installs one rooted dispatcher on EventLoop.
Only after that installation does the process facade admit the four implemented
error/rejection events. The dispatcher uses captured EventEmitter operations and
the actual process object, preserving listener order and identity. No public host
callback setter or Node/V8 runtime is introduced.

EventLoop routes native callback and job failures through uncaughtExceptionMonitor
then uncaughtException. No handler preserves terminal status 1; a throwing fatal
handler or monitor produces status 7 without recursive delivery. Ordinary rejection
notification failures instead enter the normal uncaught-exception path and can be
handled. Delivery marks the loop busy, preventing a nested GUI pump from reentering
the VM while application listeners are executing.

The LibJS rejection hook records pending Promises. Checkpoint processing also
consults is_handled because engine-internal await consumers do not always issue
a second host notification. Pending batches root the Promise and reason across
listener side effects and GC. Previously reported Promises use weak bookkeeping;
late handling queues one rooted rejectionHandled notification and then retires
the metadata. A catch installed during notification does not mutate the active
batch or invoke notification synchronously inside catch.

Runtime explicitly hands its ESM entry completion to EventLoop before driving
jobs. A rejected entry is promoted directly with origin unhandledRejection,
bypassing ordinary rejection listeners. Runtime no longer reports that completion
again after a real exception listener handles it. CJS/eval throws use synchronous
origin and continue driving remaining owned work after recovery. Pending entry
TLA status 13 and pre-application bootstrap failures remain distinct.

Tests must use actual child processes, native callbacks and Promise identities,
not a manually emitted event or a factory mock. Differential cases cover recovery,
monitor/once/prepend ordering, default/fatal exits, later catches, ESM/CJS/eval
entry failures, primitive promotion and GC during callbacks. Broader process
lifecycle, warning output, capture callbacks and strict-mode flags remain open.
Ladybird remains pinned and unmodified; no renderer dependency enters Mite.
