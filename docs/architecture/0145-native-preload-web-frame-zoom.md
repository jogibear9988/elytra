# ADR 0145: Native document-bound webFrame zoom

Status: accepted for locally tested macOS native/helper behavior.

## Context

The pinned Code-OSS preload calls webFrame.setZoomLevel after resolving its window
configuration. Sandbox process metadata and IPC do not implement that rendering
contract. Ladybird already has a browser zoom handshake between its page client,
parent view, hosting pages and compositor; no downstream engine patch is needed.

## Decision

Add a small C++ preload service with Electron's four factor/level getter/setter
names. Read actual page-client state; convert levels using powers/logarithms of
1.2. Use the upstream page_did_set_browser_zoom parent-view handshake, so the
native input mapping, CSS viewport and compositor share the same factor. Do not
store a parallel preference or apply an author CSS transform.

Capture weak source owner/document references in native functions. Retain both
locally during a call, require the live executing source realm and active document,
and revalidate after the handshake's nested event loop. Install only in configured
product top-level preloads. Freeze the capability object, block its raw functions
from bridge publication and redact the marked whole module; application wrappers
remain the way to expose narrow zoom operations.

Accept finite primitive numeric arguments only; reject coercion hooks and values
outside the native factor range 0.3..5 instead of silently clamping. This is a
recorded partial-compatibility policy. Zoom is view-scoped; Chromium's global
same-origin propagation and complete webFrame APIs are not promised. The existing
upstream synchronous handshake has no newly proven unresponsive-parent timeout.
Mite and Ladybird remain unmodified by this slice.

## Evidence and remaining work

The real engine isolation suite passes direct numeric validation, NaN/infinity/
object refusal without coercion, native export blocking and module freezing. The
actual production helper passes early preload zoom, factor/level conversion,
synchronous viewport/DPR changes, invalid inputs, page isolation, module redaction,
reload, exact native factor boundaries, asynchronous config-IPC source jobs and an
independent same-origin window. Electron 45.2.3 passes the supported
numeric/layout/reload contract with Firefly-specific policies separate.

Initial hidden tests waited on requestAnimationFrame/timers and did not finish
within the fixture deadline. Synchronous viewport/DPR reads after the actual parent
acknowledgement establish the intended API behavior without requiring visible
render frames or repeated physical UI automation.

The full existing public preload/reference regression also passes. This does not
supply webUtils/file mapping, process events/metrics, MessagePorts, workers or
protocol cache/resource contracts. The real Code-OSS entry's allowServiceWorkers
blocker and workbench open/edit/save milestone remain open. Fresh Linux/native
Windows execution remains unverified. The full product goal stays active.
