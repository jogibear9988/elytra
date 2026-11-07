# ADR 0117: Native provider seam in actual scheme fetch

Status: local macOS non-GUI component accepted. Product/helper routing and public
protocol support remain unimplemented.

## Decision

Extend the existing pinned Fetching.cpp composition with one guarded call in
scheme_fetch after upstream cancellation checks and before its scheme switch.
Keep the vendor tree unchanged. Require the full source fingerprint, exactly one
occurrence of each seam, rejection of missing/duplicate seams and restoration of
every upstream byte after removing additions. Ordinary fetch policy and response
filtering stay in upstream code.

Dispatch defaults to no provider. A native Registration owns a thread-local slot;
duplicate/empty installation fails, destruction revokes and callbacks retain their
provider through reentrant revocation. JavaScript has no registration entry point.
Only explicitly registered standard schemes are considered. Builtins and unknown
schemes continue through upstream. Require a nonopaque same-origin target, request
origin and client snapshot/creation URL, a top-level Window, and GET/HEAD without
Range. Reject navigation, worker/worklet destinations and WebSocket mode. This is
an intentionally narrow starting scope, not Electron protocol compatibility.

The provider returns the existing rooted native response adapter, which maps
metadata and credit-paced frames to real PendingResponse/body-channel objects.
Revocation during a callback stops the returned response. The owning service must
also stop its existing requests on teardown; this slot does not track active jobs
or supply document authorization, deadlines, request/credit IPC or route handlers.
Product and diagnostic helpers currently install no provider.

## Evidence and limits

check-firefly-protocol-fetch now executes the real global JavaScript fetch in a
principal realm/document against a native fixture. It reads an authorized real
4 MiB + 19 byte file through FileRequests, encoded frames, PendingResponse and
arrayBuffer, checking every byte and actual response-header filtering. HEAD,
AbortController with a pending file job, scope refusal before provider dispatch,
absence/revocation/duplicate registration and unchanged upstream data: fetch are
checked. Reentrant revocation cancels exactly once and releases native file jobs;
destroyed registration slots can be reused. No process/require/firefly/electron
page globals appear. The Python gate requires the JavaScript acceptance marker.

The fixture creates no desktop window and opens files only from its CLI-authorized
path. Its provider and consumer are in the same process. This does not accept a
live renderer connection, page navigation, CSP/cookie/COOP/COEP protocol policy,
cross-origin access, workers or native Windows/Linux execution. Those remain
separate gates. The test harness saves/restores the principal test realm's baseline
execution context around final event pumping, as HTML spin_until does; engine
microtask assertions remain intact.

Next bind the real helper/document to a bounded authorized request service, carry
request/credit/cancel messages across native IPC and enforce teardown/deadlines.
Then implement the required public protocol/session contract and re-run the pinned
Code-OSS bootstrap. A working JavaScript component fetch does not resolve its
missing Electron protocol export or constitute a workbench launch.
