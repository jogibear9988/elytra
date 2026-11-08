# ADR 0136: Bounded renderer-to-main send events with private delivery credit

Status: accepted for the local macOS top-level preload subset.

## Context

The actual pinned Code-OSS sandbox preload forwards send/invoke/on/once/removeListener.
Firefly had only invoke/handle and an inherited main EventEmitter, with no native
send route. Binary transport also needs document/realm/epoch binding before public
use. Establish a real public event path without claiming unfinished byte/port APIs.

## Decision

Add ipcRenderer.send to the existing owned endpoint and reserved native transport
seam. Share capture validation, monotonic IDs, document/realm/window authentication,
canonical-history ordering and pending budgets with invoke. The trusted event kind
comes from the reserved native route name, not an application-controlled field.
Reject invalid local inputs synchronously and return undefined for admitted sends.
Do not allocate a reply Promise. Keep a private outstanding event record until a
native ACK, timeout or document teardown releases credit.

Validate event wire graphs before main VM dispatch, and refuse invalid argument
envelopes in the main factory before calling EventEmitter. Retain native quota through
history ordering and busy-VM retries, release it before application listeners and
revalidate before ACK. Dispatch to the actual ipcMain EventEmitter with the shared
native sender snapshot. Preserve registration order/once/removal/receiver semantics;
ignore listener results, and retain ordinary listener exception behavior. Supply
Electron's default no-op error-channel listener. Unknown channels succeed without
application callbacks. Native ACKs acknowledge dispatch, never application results.

Local limits throw, later native admission/deadline failures produce explicit
helper diagnostics. Document these bounded alpha differences. Close/navigation/crash
cleanup still owns all reservations. Block raw send publication through contextBridge
just as for invoke; only fixed application wrappers should enter the principal world.

No upstream code is changed. The main JS factory, native window bridge and owned
renderer helper are rebuilt together when this private signature changes.

## Acceptance and limits

Actual public macOS acceptance batches one hidden window with document reloads:
early sends, interleaved invokes, order/once, cycles/UTF-16/mutability, trap-free
synchronous validation, raw publication refusal, native history URL, sender identity,
16 queued events, ACK credit recovery and window destruction from a listener.
Node and actual Mite run the shared factory/EventEmitter policy without windows.
A real Electron reference covers the positive early/order/clone/sender/reload/close
contract, separately from Firefly-specific quotas and document snapshots.

Fresh hosted Linux execution and native Windows desktop remain open. Main-to-
renderer events/listeners, event.reply, sync IPC, binary public values, MessagePorts,
subframes and general structured clone remain unimplemented. The private ACK is
not a user-visible reply channel. This is a required Code-OSS preload prerequisite,
not a successful workbench launch; the actual entry remains blocked earlier by
unsupported protocol worker/cache/remote-scheme contracts.
