# ADR 0122: Real helper navigation over a trusted resource channel

Status: local macOS Release native headless acceptance. A real production helper
loads and activates a custom-origin document; public protocol/session APIs and
post-commit resource loading remain unimplemented.

## Decision

Bind the native navigation client before upstream starts creating navigation
params. Extend the existing guarded WebContentPage composition with one call
immediately before async_create_navigation_params. At that point the native UI
already owns the exact population request and the helper has adopted its ID.
The optional native handler may send the endpoint first over the same control
connection, then upstream sends the original request unchanged. Preserve the
complete source fingerprint, both unique seams, exact upstream-byte restoration
and missing/duplicate seam rejection. Retain the page/loader through the optional
callback, keep a weak native navigable, and recheck openness/current navigation/
worker/loader before continuing: a callback may cancel or replace its navigation. No vendor modification or general page API.

Append FireflyConnectNavigation after the existing private byte handoff using the
unchanged pinned IPC generator. Preserve every original ID and the first private
byte message ID; the new method is the second append. Its NotRouted default stub
forwards to an optional LibIPC callback with the receiving stub and a moved native
handle. The callback's registration/lifetime lives in one LibIPC instance, without
introducing a LibWebCommon dependency into that library. Fields are page number,
navigable namespace/local ID, native navigation ID, expected URL and nonzero epoch.
Reject zero identity/epoch and empty/over-budget ID/URL before helper admission.
The native caller remains responsible for granting this channel to that intent.

Owned helper startup registers a resource receiver after VM initialization.
No provider is installed until a handoff passes validation. The receiver is called
only by the actual ConnectionFromClient server stub in this owned entry; inspect
that pinned inheritance on updates. Look up the page in that connection's PageHost
and the navigable in that page, then validate the current top-level Navigation
scope. Do not perform a global page/realm lookup or trust a page-reported URL.
The service runs on its owning agent thread. Use the staged LibIPC declaration,
also used by the generated endpoint, to avoid duplicate source/SDK declarations.

Require strictly increasing epochs per helper. A duplicate navigation ID never
replaces its original channel. Keep at most 32 live native-navigable records,
remove destroyed ones, and retain the last ID for each live root to prevent replay.
A 250-ms native sweep revokes finished clients and releases their VM/channel state;
unused admissions expire after five seconds. Startup installs only a navigation
handler, so no post-commit document fetch capability is granted. Shutdown removes
the receiver/provider and revokes clients. The parent still authorizes each actual
file path with the existing checked-file service.

C++ directly integrates the existing native UI/control/GC/IPC boundary. No extra
FFI, JavaScript transport, engine-origin override or body substitution is needed.
Mite remains independent and Ladybird stays unmodified.

## Evidence and limits

check-firefly-resource-renderer uses the actual HeadlessWebView/UI navigation and
the production owned helper. Only its native parent knows the CLI-authorized
Unicode HTML filename. It grants the channel at the new hook, serves the file via
FileServer, and lets upstream populate, transfer, commit and parse the document.
No test assigns document URL/origin, calls the population algorithm directly or
publishes a fake location getter.

The executed page script reports its real app://bundle origin, secure context and
absent process/require/firefly/electron globals. The exact marker cannot come from
the HTML source alone. Require distinct native parent/renderer PIDs and a ready
marker from the actual source helper. Duplicate admission closes its extra endpoint
while the original document still loads with exactly one file admission. Wrong
navigation ID, wrong page and missing handoff create no file admissions/provider.
The document/control connection stays alive in every control case. A reentrant
cancellation starts a real replacement data: document from the hook, keeps the
borrowed original request valid, and produces no stale app: file admission.

The probe waits for the helper itself to retire the completed channel and requires
zero pending main-side jobs/files. Then request the document again to prove it
survives retirement. Queue the second observation after the first callback: the
upstream ViewImplementation clears its single pending page-info slot only after
that callback returns. Do not weaken this check or change upstream assertions.

Require the gate in check-firefly/macOS Release and the experimental Linux runtime
workflow. Existing native navigation population, JavaScript/process fetch, actual
helper policy/DOM-origin and diagnostic byte handoff regressions still pass locally.
All these tests are headless and create no desktop windows. Fresh native Linux
execution remains required; the experimental Windows production helper is not
enabled by this step. Shared control code still needs fresh Windows compilation.

This proves an internal native navigation, not BrowserWindow.loadURL custom scheme
support through the public runtime. The normal main bridge installs no file-route
authorizer, and the helper's document handler remains empty. Next bind a new exact
document to post-commit resource transport before its scripts/styles need it,
including selected-process changes, navigation/teardown and security checks.
Then implement the demanded public protocol/session APIs and re-run real Code-OSS
startup. A native custom-origin DOM load is not an accepted workbench launch.
