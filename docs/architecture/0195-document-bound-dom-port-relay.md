# ADR 0195: Document-bound native DOM port relay

Status: focused macOS offscreen acceptance; actual Code-OSS watcher channel connects.
Editor readiness and other platforms remain unaccepted.

## Decision

Reuse the native FUP1 codec and bounded ledger for the renderer leg. Main-side
C++ owns document/realm/epoch authority and transactional transfer admission.
The helper creates genuine SDK MessagePort pairs in the isolated source realm,
delivers endpoints in event.ports, and permits the normal window.postMessage
transfer into the principal realm. The JavaScript main ownership core brokers a
utility-backed peer to the document link; utility-to-utility forwarding is refused.
No Node/V8, public route identifiers or JavaScript-shaped DOM ports are involved.

Use distinct application port-control and transfer-ACK frame kinds with independent
sequence and credit accounting. A transfer shares the bounded ordinary outbound
window, but its ACK cannot acknowledge unrelated IPC. Each route has 16 messages /
1 MiB credit per direction. Private controls and pending document wires remain
bounded. Stale epoch/generation/channel traffic cannot address a newer document.
Navigation, crash and teardown revoke the link and native observations.

## Actual queue consumption

The SDK's public MessagePort API does not expose dequeue/terminal observation.
Refunding credit after post_message merely moves an unbounded queue into LibWeb;
JavaScript listeners also miss consumption without listeners and legitimate
window.postMessage transfer. Add an exact hash-pinned reversible Rust source
composition through the existing owned compiler driver, leaving vendor unchanged.
C++ is used for the native ledger/helper/weak observer registry; the minimal Rust
composition touches the SDK's actual Rust queue and transfer lifecycle.

Native observation carries an opaque monotonic token plus random process-origin
nonce. The registry checks owning thread/VM before foreign GC access, then exact
source/principal realm and responsible document. States contain atomic dequeue and
close counters, no JavaScript callbacks or GC roots. Legitimate transfer moves
observation before old-object finalization. Consumption is counted at actual SDK
queue removal; undelivered/discarded messages never produce consumption credits.
A separate forwarding terminal state closes only after preceding delivery tasks.
The helper revokes before destroying the authority context.

## Evidence and limits

The actual Rust/native Release host and static-offscreen builds pass. The focused
public fixture passes main-to-principal and utility-to-principal handoff, delayed
start, 32 independent 4 KiB snapshots, FIFO, detached-owner no-ops, ordered close
and actual utility PID retirement. A separate no-listener/GC observer probe has
strict compile evidence; its runtime remains unaccepted at this checkpoint.

The actual pinned Code-OSS entry now establishes the renderer/file-watcher channel.
That does not establish SharedProcess service readiness or a usable editor. Reverse
renderer attachment through ipcRenderer.postMessage, child-to-parent attachment,
nested native transfer, utility-to-utility forwarding and larger-file performance
remain outside this slice. The public binary graph limit remains 64 KiB; no zero-copy
or Linux/Windows/package acceptance is claimed.
