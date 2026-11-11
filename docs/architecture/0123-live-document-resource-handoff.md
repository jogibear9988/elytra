# ADR 0123: Bind resource handoffs to the actual committed document

Status: implemented with local macOS Release headless production-helper acceptance.
Public protocol/session handlers and a Code-OSS workbench launch remain open.

Subsequent public default-session file integration is recorded in
[ADR 0126](0126-public-file-protocol-routes.md). The status below describes this step.
[ADR 0175](0175-public-response-protocols.md) later superseded the blanket
duplicate-ID rule: a trusted higher-epoch grant can refresh the same canonical
document binding for late file interception. Equal-epoch replay remains rejected.
The headless resource gate now distinguishes replay rejection from a genuine
higher-epoch refresh with resource delivery and retirement of the old channel.

## Decision

Extend the existing guarded WebContentPage composition with a third unique seam,
immediately before the selected host's async_populate_navigation. Upstream has
already assigned the destination document ID. The optional native callback receives
that real population result and can transfer a resource endpoint before the original
population message on the same control connection. Retain the loader and weakly
track the native navigable across callback reentry; revalidate the host and current
navigation/loader before continuing. Preserve the full source fingerprint, exact
byte restoration and missing/duplicate seam rejection. Ladybird remains unmodified.

Append FireflyConnectDocument after the private byte and navigation messages using
the pinned upstream generator. All original IDs and the previous two private IDs
remain unchanged. The third NotRouted message carries the navigation binding plus
native document namespace/local ID. Use the same canonical staged LibIPC declaration
and retained optional callback ownership as the preceding handoffs.

The owned helper accepts pending transports only on its owning agent thread and
for a page in the actual receiving ConnectionFromClient/PageHost. Require a valid
registered standard nonopaque URL, nonzero identities and strictly increasing
helper epoch. Bound the document map to 32 entries. Reject duplicate document IDs;
a second endpoint cannot replace the original. Unused grants expire after five
seconds. No document resource capability exists without an explicit native grant.

The selected host may not have created its document yet when it receives the grant.
Therefore bind lazily on the first eligible native resource request, resolving the
actual local Window/settings from the engine's request. Match the native document
ID, page, top-level navigable ID, expected document URL and origin, and require the
document to be fully active. Do not grant the initial about:blank or another
same-origin document. Create the existing document client only after these checks;
it additionally enforces the exact native settings identity on every request.

Keep the channel across idle periods while this document remains active. A 250-ms
sweep revokes inactive document clients; helper shutdown revokes every client and
pending transport and removes both receivers and the provider. Preserve the existing
bounded request queue, byte credit, file ownership, cancellation and real 30-second
request deadlines. Navigation clients still retire independently after population.
Mite stays independent of LibWeb and Firefly.

## Evidence and limits

check-firefly-resource-renderer now also runs test_document_resources.py. Its real
HeadlessWebView/UI loads a Unicode-path HTML file through the production helper,
then external ESM (including a second module), CSS, a 4-MiB-plus binary GET, HEAD and
an explicitly refused route. Only the native parent authorizes fixed file paths;
renderer URLs never become filesystem paths. The script checks every binary byte,
empty HEAD body/advertised length, CSS application, secure app://bundle origin,
filtered Set-Cookie and absent process/require/firefly/electron globals.

The test requires native parent/renderer PID evidence and one document-ready marker
from the actual selected helper. Wrong document ID and no grant admit zero resource
files. Duplicate admission closes only the extra endpoint. Navigating to a real
data: replacement document closes the old document channel with no pending native
jobs/files; a follow-up DOM observation proves the control path remains alive.
These tests create no desktop windows. Local macOS Release passes; fresh hosted
Linux execution and shared Windows compilation are still required. The Windows
production helper remains unsupported.

The authorizer in this probe is test-only. The normal main bridge has no public
protocol/session route registration or automatic document grant. This does not make
BrowserWindow.loadURL custom protocols usable through the public runtime and does
not prove Code-OSS startup. Next connect those public contracts to this native path,
then rerun the pinned actual main entry and follow its next real failure.
