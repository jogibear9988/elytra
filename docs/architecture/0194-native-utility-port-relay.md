# ADR 0194: Native utility port relay

Status: initial main-to-utility implementation, with actual local macOS process
acceptance. Renderer/DOM, reverse and nested native transfers remain open.

## Demand

Code-OSS creates MessageChannelMain pairs, moves an endpoint into a utility and
later gives its peer to a renderer. The existing local JavaScript queues cannot
cross a process boundary. A serializable public port identifier would neither
transfer ownership nor create the DOM MessagePort required by window.postMessage.

## Decision

Extend Firefly's existing private utility pipe with a native route ledger and
versioned FUP1 envelopes. Use C++ here because the subprocess owner, libuv channel,
LibJS host calls and teardown already live in C++; a Rust FFI adds no benefit to
this bounded adapter. Keep the codec/accounting independent of LibJS and Qt.
Neither Mite's architecture nor Ladybird sources are modified.

The pipe retains its existing four-byte length framing, 128 KiB frame limit and
16-write/2 MiB admission bound. Legacy payload frames remain unchanged. FUP1 uses
a 16-byte header containing kind, attachment count, reserved bits and native route
identity/generation. Transfer frames carry up to 16 native-issued identities plus
the existing application graph. Data, exact consumption ACK and close frames use
only routes granted on that private child channel. A child cannot introduce routes;
unknown/future identities or malformed credit messages fail closed. Closed issued
identities remain recognizable while the pipe drains and cannot be reopened.

The sender changes JavaScript ownership only after native write admission. Transfer
list reentry is validated before admission. Receiver wrapper construction is staged
before rechecking lifetime and endpoint capacity. Old wrappers become inert, queued
data stays with the moved endpoint and native route metadata never enters user data.

Each route has 16 messages/1 MiB outstanding per direction. Each child reserves
4 MiB transmit plus 4 MiB receive credit, allowing simultaneous duplex bursts within
8 MiB. ACKs must match exact FIFO byte counts; they are sent after JavaScript
consumption. Native writable notifications retry blocked data, ACK and close writes
without redelivering messages. The existing wrapper limits still apply: 256 endpoint
records, 64 queued messages/1 MiB per endpoint and 4 MiB globally in that realm.

Explicit close preserves accepted queued traffic ahead of peer close. Channel loss
and child exit revoke routes; messages unread at process termination are not promised
to survive. Weak wrappers/record leases and native ledger ownership remain separate.
The application never receives transferable integer file descriptors. This is a
copied broker relay, not OS-handle transfer or zero-copy file transport.

## Acceptance and remaining work

The actual no-window process fixture verifies failed native admission retaining the
old owner, queued pre-transfer data, 32 independent binary snapshots under delayed
start, bidirectional FIFO, peer close and child identity/exit. The pure native probe
covers malformed/replayed identities, exact credits, duplex budgets and retirement.
This is not a measured throughput or cross-platform acceptance result.

Only initial main-to-utility attachment is implemented. Child-to-parent attachment,
nested attachments, queued transferred ports and forwarding native-backed endpoints
remain explicit refusals. Renderer delivery needs genuine LibWeb DOM MessagePorts
and native document/epoch authentication. The 64 KiB binary graph budget remains
separate; larger-file transport still needs implementation and measurement.
