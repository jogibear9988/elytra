# 0026: Own a renderer-side isolated wrapper world

Status: internal synchronous LibWeb adapter and native gate implemented on macOS
ARM64. Not installed into the Firefly renderer yet; public preload/contextBridge
and renderer IPC remain unsupported.

## Decision

Use LibWeb's actual Extension WrapperWorld, HostDefined, Intrinsics and per-world
WindowProxy APIs to create a separate realm for a document's existing Window.
Keep the same DOM implementation objects while obtaining distinct JS globals,
prototypes and wrapper expandos. Follow the public construction sequence exercised
by the pinned upstream TestWindowProxyWorld; do not copy or modify engine sources.
The new owner lives under firefly/renderer-bridge and compiles separately from both
the browser source and standalone Mite. Its JS target is the one matching LibWeb.
The executable participates in upstream's Skia confinement and integrity checks.

The adapter takes the principal environment's agent-cluster isolation mode instead
of adding a capability such as SharedArrayBuffer that the document lacks. The gate
currently exercises an about:blank document; nondefault COI pages still need native
integration tests. No filesystem/process/Buffer binding is installed. The test
places only a harmless native marker function in one world to check that the page
and another world cannot reach it through selected global/constructor paths.

Use a reference-counted C++ owner with explicit GC roots for the realm and world,
and an owned execution context. All operations run on the VM owner thread with an
active caller context. Creation rejects a Window without a document browsing
context. Each synchronous evaluation pushes its realm context and restores the
caller context for success, thrown exceptions and syntax errors. Recursive calls
to evaluate on the same owner fail explicitly.

Detach is idempotent. A world already detached through the SDK also rejects
owner evaluation. It calls the upstream world's detach API before releasing
context/roots. When requested by a native callback during evaluation, it marks
the owner logically detached immediately, denies further evaluation/realm access
and defers physical teardown until the active context is popped. Evaluation holds
a strong self-reference, so releasing the last external owner inside that callback
does not destroy its active frame. Destructor cleanup uses the same detach path.

## Proof and its limits

The probe creates a real LibWeb Page/traversable/document using the existing
principal-realm helper. It executes scripts in the page and two separate owned
worlds. Check poisoned Object/Document prototypes and document expandos, shared
DOM attribute changes, independent world globals, absence of Mite globals and a
native marker restricted to one world. Check WindowProxy self/top/parent,
document.defaultView and Window/Document/element constructor realm paths.

Force GC while a world is rooted, detach one without changing the other, create
a fresh world after page prototype pollution and then detach from a running native
callback while releasing its external owner. Require correct context depth,
logical revocation, deferred physical detach and successful final GC/destruction.
This is an actual single-process LibWeb/LibJS gate, not an Electron comparison or
a renderer-helper/protocol test. It can run without a visible desktop.

The adapter is not a complete sandbox or a contextBridge membrane. Its internal
C++ evaluate result is a raw VM value for trusted renderer-side callers; it must
never be returned directly across the future world/IPC boundary. Retained
functions, DOM listeners, promise jobs and timers are not automatically revoked
by this owner. Before final teardown, the future renderer helper must stop or
revoke those activities and handle document generations/navigation/crashes.
Async callback lifecycle and capability membranes need separate adversarial tests.
No public preload option is enabled by this work.

## Next integration gate

Attach ownership to the actual renderer's before-author-script document lifecycle,
using the EventSink/session adapter investigated in ADR 0025, and prove it runs in
the correct process/document before any page code. Add bounded structured copies,
function/Promise proxies and generation-based revocation, then validated IPC with
sender/frame checks and budgets. Public Electron-style preload/contextBridge APIs
must wait for these tests. No upstream patch stack or new native platform support
is claimed. Windows/Linux, nondefault COI and the locked-desktop presentation gate
remain outstanding.
