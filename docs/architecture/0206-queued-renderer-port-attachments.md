# ADR 0206: Queue renderer port attachments with ordinary events

Status: implemented; focused local macOS offscreen acceptance, 2026-11-17.

## Problem

A real Code-OSS run refused a 364-byte attachment while sixteen ordinary events
awaited renderer dispatch ACKs, even though its physical byte queue was empty.
Ordinary webContents.send already waited within bounded native queues; port
attachments instead rejected immediately at the active window. A passing terminal
workflow did not establish reliable delivery for that startup burst.

## Decision

Use the same FIFO waiting-event queue for immutable encoded attachments and
ordinary events. Keep all existing bounds: 256 waiting/document, 1024 globally,
16 active/document, 64 globally, and the existing wire-byte leases/deadlines.
Allocate the complete JS result before admission and reserve route identities and
one wire lease before successful ownership transfer. Failed admission leaves the
original wrappers attached. Move that same lease into the physical queue once;
assign the outer port-control sequence when the attachment actually promotes.

Reserved routes cannot send DATA/CLOSE before promotion. Incoming traffic for a
reserved route fails closed before ledger mutation. Promotion schedules the
existing owner-context writable notification, allowing queued data to drain.
Document/channel reset revokes reserved routes and releases waiting credit.
The first accepted physical fragment starts the existing thirty-second ACK timer.
No application retry, Code-OSS patch, enlarged quota or speculative ACK is used.

## Evidence and limits

Release firefly and firefly-view-offscreen build. One existing offscreen renderer
batch passes six cases: original DOM FIFO/burst/utility relay, sixteen ordinary
events followed by a real DOM handoff in one main task, full bounded backlog
refusal with original ownership then recovery, and queued attachment cancellation
on window destruction followed by a fresh document exchange.
Evidence: build/queued-port-{firefly-build,renderer-controls}.log.

The reused Code-OSS stage also passes original CommonJS extension activation,
command, editor edit/save and a real Bash terminal/sendText/exit17 without the
diagnostic flag. All fourteen observed native processes stop with zero cleanup
signals. Evidence: build/codeoss-queued-port-{acceptance.log,evidence.json,
workbench.log}. These establish the tested workflows, not arbitrary extensions,
visible keyboard/terminal painting, fresh Linux/Windows or general saturation
liveness. Bulk port DATA competing for physical control slots remains separate
work; reserve essential Reply/ACK/CLOSE capacity within existing bounds.
