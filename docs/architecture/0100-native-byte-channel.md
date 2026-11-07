# ADR 0100: Private native byte channel with delivery credit

Status: implemented component; local macOS ARM64 native transport/process tests
pass. Linux/Windows source paths and hosted gates are present, execution pending.
Not connected to public Firefly IPC or the product WebContent/preload endpoint.

## Context and choice

The Code-OSS path needs native binary delivery, not just a graph codec. Existing
public IPC travels through BiDi/JSON and main-process strings; those fields cannot
carry arbitrary FFB2 payload bytes. Build a separate owned native channel using
the pinned LibIPC transport. Keep Ladybird unmodified and use its native Mach,
Unix-socket and Windows transport/bootstrap interfaces rather than a loopback HTTP
server, temporary file, Node process or encoded text workaround.

The channel depends on LibIPC/LibCore/AK only. Its test link and native dependency
records have no LibJS/LibWeb/LibWebView/Mite/QtWidgets inputs. No renderer, JavaScript
engine or visible window is needed for this component gate. This does not add a
Firefly dependency to standalone Mite.

## Framing and flow

FFBC is a private channel protocol, distinct from FFB2 graph serialization. Each
packet has a twenty-byte header: four magic bytes, version 1, kind Data=0/Ack=1,
two zero reserved bytes, little-endian uint64 sequence and uint32 payload length.
The exact raw payload follows. Refuse unknown versions/kinds/reserved bits, zero
sequences, mismatched lengths and payloads above 128 KiB. ACK has no payload.
Neither payload nor header uses JSON, base64 or JavaScript strings.

Maintain at most sixteen unacknowledged sends and sixteen admitted receives,
including zero-byte messages and active callback batches. Count payload bytes up
to two MiB in either direction. A send over credit returns an explicit error; the
caller can retry after on_writable. Sequences start at one and cannot wrap. Incoming
data must be consecutive; ACK must name one outstanding send. Replay, unsolicited
ACK, malformed input or unexpected native attachments closes the channel. This
byte-data protocol does not grant file/descriptor access.

Deliver callbacks on the Core owner thread after reading, preserving FIFO across
reentrant event-loop pumping. Counts include the currently dispatched batch so
nested reads cannot evade limits. A delivered native callback returns before ACK
is sent. ACK means transport callback consumption, not application handler success
or a filesystem transaction. The owning C++ peer cooperates with this rule; a
compromised peer's ACK cannot establish honest application processing.

Retain the transport's received byte storage/mapping until callback completion;
there is no extra receive-payload copy in Channel. The callback span is borrowed
only for that call. A consumer retaining bytes must copy them. Sending constructs
owned packet bytes before returning; source mutation cannot change queued data.
No overall zero-copy or measured throughput/latency claim follows from this.

Close clears queued data, credit and callbacks, and closes the native transport.
Peer loss discards pending application delivery. Callback lifetime is protected
even when a callback closes/releases its channel. Mutating APIs require the Core
owner thread. On Windows, request upstream's fail-instead-of-block pipe policy so
a stalled peer cannot block this owner in a pipe write. A native capacity failure
may close the channel before its application credit maximum is reached.

These are application-layer queue bounds. The upstream transport may allocate/map
or queue incoming data before Channel validates it; this component does not prove
a hard bound on all transport/kernel allocations under a compromised peer. Native
transport hardening and full renderer containment remain separate requirements.

## Native evidence

The actual native transport gate checks ordered/deferred data, the send-credit
boundary, ACK-driven reuse, zero-byte receive flood refusal, reentrant dispatch
accounting/order, maximum 128-KiB payload and mutation independence, unexpected
handle rejection, forged ACK/replay and real kernel EOF. An independent Python
peer constructs/validates fixed header bytes and all payload bytes, plus malformed
input; the fixture enables Windows binary stdin/stdout mode.

A separate owned executable receives its native startup transport. On macOS the
test Mach bootstrap admits only the OS-reported PID of the spawned child, under
registration/admission locks. Unix uses only the child's inherited socket end;
Windows uses the child's pipe/broker endpoints. Restore fixture environment changes
and terminate/reap owned failed children. A test-only watchdog bounds child startup
after abrupt parent loss; it is not a product sandbox or process supervisor.

The child echoes its actual OS PID plus 65,536 arbitrary bytes, then an empty
message; parent verifies the spawned PID and exact bytes. Wait for native ACKs
before normal child exit. The independent supervisor also compares parent metadata
to its real subprocess PID. Current local proof is macOS; other OS branches still
need their native executions. No AppContainer or renderer containment proof follows.

```sh
cmake --build build/renderer --target check-firefly-byte-channel --parallel 3
```

The same gate is a dependency of check-firefly-wire and the existing isolation
acceptance path. It is non-GUI; do not launch a desktop suite to check this layer.

## Product integration still required

Pass a dedicated native endpoint from the actual main process to the owned
WebContent helper through trusted control IPC. Verify process/channel ownership;
never expose bootstrap endpoints, paths or send primitives to page JavaScript.
Add document/realm/epoch/request routing and revocation, canonical sender metadata,
request deadlines and aggregate host budgets. Independent channels do not provide
ordering relative to the existing control connection: require a control-plane
fence before relying on canonical navigation/history state for authorization.

Move main-process payload handling off JS text fields, connect FFB2 capture/decode
with explicit binary policies, and define contextBridge publication semantics.
Require actual LibWeb/preload/main traffic, navigation/process replacement, stale
endpoint/forgery/pressure/crash tests and native platform gates before enabling
public binary support. Then build MessagePorts, streaming and authorized lazy file
capabilities on the accepted path. Code-OSS still does not run.
