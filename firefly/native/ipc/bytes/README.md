# Private byte transport

`Packet` implements a bounded raw-byte frame. `Channel` owns a pinned native
LibIPC transport, deferred Core-thread callbacks, sequence validation and limited
send/receive credit. It has no VM or window dependency.

The supplied transport is an owned capability; this library does not discover or
authorize peers. Callback byte spans expire when the callback returns. Use the
separate native component gate before integrating it with document endpoints.

See [ADR 0100](../../../../docs/architecture/0100-native-byte-channel.md) for limits,
actual proof and required main/WebContent integration. Public IPC is still on its
existing plain-data path.
