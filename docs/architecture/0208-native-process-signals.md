# ADR 0208: Native process signal sending

Status: implemented; local macOS CLI/embedded acceptance, 2026-11-17.

## Need

Original Code-OSS checks process.kill(parentPid, 0) once per second in its extension
host. A missing method can terminate a healthy extension host; returning a dummy
success would hide a dead parent. Other original consumers use ESRCH to distinguish
stale process/storage-lock owners. Implement the real operation in Mite rather
than patching those consumers.

## Decision

Keep public policy in a small ESM process-kill initializer. Capture the private
native sender and an own-key OS signal table before author code. Preserve Node
24.15 legacy PID/coercion ordering, signed int32 arguments, case-sensitive names,
falsy signal defaults and numeric zero. Normalize both arguments before the native
boundary. The native function accepts only int32 values and calls uv_kill directly;
root the host/error while allocating native functions and error properties. Return
true only after real success; preserve code/negative errno/syscall on failure.
Global/default/CommonJS/named process exports reference the same public function.

Sending is independent of permission to own native signal watchers. No callback,
worker, PID cache, synthetic child state or success stub answers signal-zero checks.
Process groups and Windows limitations remain native OS/libuv behavior. The private
Node _kill interception hook fails explicitly; public kill captures the real sender.
No Node/V8 dependency or Ladybird source modification is introduced.

Unlike Node's heuristic native RunAtExit before selected self/group signals, Mite
does not reset active stdio flags speculatively: signals can be handled, ignored,
nonterminating or fail. Signal-caused death stays abrupt and outside the normal
stdio teardown guarantee. No JS exit event or output flush is synthesized.

## Validation and scope

Release Mite/embedded host and Firefly/offscreen builds pass. Actual Node, Mite and
embedded-host controls each pass six exclusively owned child modes with real
termination/reaping before ESRCH, identity and coercion checks, and self/parent/
owned-group signal-zero checks. Existing watcher/policy/handler restoration tests
pass. Logs: build/process-kill-{controls,watcher-controls}.log.

Fresh native Linux/Windows, nonzero group delivery and every OS signal are not
runtime-accepted by the local macOS slice. The extended real Code-OSS parent-watch
workflow passes separately: three 1100 ms continuations each verify the native
parent and execute the original extension command, with the saved editor coherent,
followed by real terminal creation/sendText/exit17. All fifteen observed native
processes stop without cleanup signals. This is controlled offscreen acceptance,
not general visible desktop/extension compatibility. Evidence:
build/codeoss-parent-liveness-{acceptance.log,evidence.json,workbench.log}.

References: [Node 24.15 public policy](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/process/per_thread.js),
[Node native kill/RunAtExit](https://github.com/nodejs/node/blob/v24.15.0/src/node_process_methods.cc).
