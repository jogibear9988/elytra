# Native transport seam

TransportHooks routes reserved Firefly events from LibWebView to the window bridge.
The owned WebContentPage adapter is generated with a pinned source fingerprint;
Ladybird stays unmodified. Unknown reserved messages are consumed rather than
forwarded to the optional WebDriver observer. Clear the handler before destroying
the host VM or its windows.

The Qt Bridge validates canonical page/navigable/document ownership, tracks bounded
pending request IDs, waits for the Mite entry boundary and revalidates replies.
Renderer endpoint and Promise ownership live in renderer-helper/ipc; the command
composition seam lives in renderer-helper/UpstreamSessionAdapter.cpp. The wire graph
codec lives in renderer-bridge/serialization and admits no function tokens.

See ADR 0072 and docs/api/ipc.md for scheduling, limits and unsupported modes.
