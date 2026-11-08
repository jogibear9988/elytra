# ADR 0155: Real LibJS utility children before cross-process port handoff

Status: implemented and locally tested on macOS with real Firefly/Electron. Native utility
MessagePort and renderer handoff remain unfinished.

## Context

The actual Code-OSS main graph imports utilityProcess after MessageChannelMain.
Its utility wrapper calls fork on bootstrap-fork.js, immediately sends configuration,
attaches stdout/stderr, and observes spawn/message/exit. The shared process later
accepts a genuine MessagePort endpoint and wraps it in the unchanged binary IPC
Protocol. An export that only resolves imports or echoes configuration cannot
satisfy this sequence. A child must execute actual JavaScript in an independent
LibJS process, and unsupported connections must remain explicit.

## Decision

Launch the same executable using a private --firefly-utility mode selected before
Qt/application/window startup. Remove the internal mode switch before Mite receives
modulePath and arguments. Run Mite's independent LibJS runtime/event loop with a
Firefly-owned initialization hook that installs process.parentPort and process.type
utility. No Node/V8 process or Electron identity is substituted. Mite remains
independent of Firefly and LibWeb; the utility executable integration owns the host.

Use a libuv-created private inherited duplex pipe at descriptor 3. It belongs to the
spawned process relationship, rather than a public listening socket or a serialized
application-supplied port ID. Frames contain a four-byte length followed by bounded
native BridgeData wire bytes. Capture/materialize the same mutable copied binary
graph as public IPC, including aliases and cycles; preserve the public aggregate
64 KiB distinct backing-store bound and 128 KiB encoded frame limit. No JSON clone
or ArrayBuffer ownership transfer is introduced.

Bound each native endpoint to 16 pending writes / 2 MiB framed bytes and the parent
service to 32 owned children. The child facade separately bounds incoming queued
data to 64 messages / 1 MiB encoded bytes. Overflow, invalid native data and failed
writes report explicit errors, close unusable channels where appropriate and never
claim successful delivery. Callback roots and endpoint/handle ownership survive
asynchronous native callbacks; shutdown disables JavaScript dispatch, closes handles
and terminates owned processes for native reaping.

Expose a maintained ESM utility-process facade, with env/cwd/args and native
stdout/stderr inherit/ignore/pipe behavior. Keep piped output separate from message
framing and cap unconsumed Readable output at 1 MiB. Spawn establishes the PID;
exit clears it; pipe EOF retires the respective stream independently. ParentPort
queues before listener attachment, then schedules FIFO events containing data and
an empty ports array. Child replies deliver raw values to the parent's message
event. The real reference confirms idle children remain alive even without listeners.
Natural exit 7, final frames and kill/PID retirement pass; exact task timing and full
Electron event/crash parity remain open.

Reject engine flags, unsigned-library loading, authentication forwarding, unknown
options, service naming and nonempty transfer lists. Electron crash attribution,
app process metrics and child-process-gone events are not implemented. Do not silently enable
unimplemented network/authentication or native-addon behavior just to advance a
Code-OSS import or launch call.

## Evidence and remaining work

The fresh actual Code-OSS ready/main probe completes its entry and reaches app
ready; the next missing static export is safeStorage. This is import/startup
evidence, not execution of the utility fork, authentication option or port handoff.
Real public Firefly/Electron tests establish same-executable child identity, queued
binary graphs, args/env/cwd, stdio, natural exit/final frames, kill, eight rapid
module failures and reaping a live child on parent exit. Six real headless malformed/
missing-channel gates pass. The missing-fd negative control exposed libuv reusing
fd 3 before initialization; validate the inherited descriptor before Mite starts.
An idempotent spawn announcement also prevents a fast exit from overtaking its
startup timer and later resurrecting the public PID. Native Linux/Windows and actual
Code-OSS utility startup remain unaccepted.

The utility fixture uncovered missing Mite stdout/stderr globals: add independent
Writable adapters using existing asynchronous native fs.write, including partial
writes and borrowed fd ownership. Real Node/Mite raw-byte and short-write fixtures
pass; stdin/TTY/full stdio remain unavailable. No window or desktop input automation
is used by these component tests. Exercise actual pinned
Code-OSS utility components only with their genuine dependencies; importing the
wrapper or a synthetic handshake alone is not shared-process acceptance.

Next bind the existing main MessagePort ownership core to real child/native endpoint
handoff, maintaining process identity, transfer commit, detachment, queued frames
and close/disconnect lifecycle. Then bind renderer/document endpoints separately.
The actual shared-process default additionally requests authentication forwarding;
that requirement remains unsupported. bootstrap-fork's LibJS adaptations and its
real entrypoint/service graph must still run before claiming a shared process,
extension host or workbench. Ladybird remains unmodified.

See the [utilityProcess API scope](../api/utility-process.md),
[ADR 0154](0154-main-message-port-ownership.md) and
[current Code-OSS status](../compatibility/vscode.md).
