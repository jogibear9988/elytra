# Native scalar defaults, passive notifications and media status

Status: partial macOS implementation. Controlled policy/Host/provider, genuine
read-only queries and isolated native suite/local-center checks pass. Physical
system events, default-domain persistence and capture permission workflows remain
unaccepted. Native Windows/Linux providers fail explicitly.

## Decision

Use NSUserDefaults for string/boolean getters/setters and Foundation/AppKit native
notification centers for distributed, local and workspace observation. Keep each
subscription's center/token and plain callback state owned by the native provider.
Missing string/boolean values follow real native getters; other types fail instead
of becoming fabricated zero/empty values. Do not implement platform preferences
through process commands or write global preference domains indirectly.

Read camera/microphone status from AVFoundation and screen availability through
CoreGraphics preflight. A negative screen preflight cannot distinguish denial
from not-yet-requested access, so report unknown. No status query requests access
or grants renderer privileges; prompting/posting and other unsupported APIs fail
explicitly. Preserve Ladybird unchanged and Mite independent.

## Ownership and validation

Subscription IDs begin at zero and increase across all three families per host.
Unknown IDs and mismatched-family removals are no-ops. Register before native
admission, then retire before removal/shutdown; failed native starts discard their
queued events. JS retains callbacks only while active, with bounded early FIFO
packets and detached validated JSON dictionaries. Failed late admission receives
another best-effort removal after a reentrant shutdown.

A process-global 64-observer ticket budget remains charged until actual native
removal succeeds, including failed cleanup/host churn. Inactive failed-removal
records retain owned tokens for bounded teardown retry rather than claiming that
removal succeeded. Native callbacks hold weak plain state and queue no VM or GC
root. The owner queue permits 128 normal notifications overall and 64 per observer;
overflow/corrupt records produce one terminal error and revoke that subscription.
Stop revokes the receiver under the posting mutex, then calls native cleanup
outside it. Materialize Qt JSON values before temporary containers disappear.

Bound native identifiers/text/JSON, validate UTF-8/UTF-16, typed values, node count
and graph depth. UserInfo JSON data can preserve escaped NUL; names, senders and
scalar defaults cannot. Native NSString senders copy to strings; other sender
objects yield the Electron-compatible empty string. No malformed packet creates
successful callback arguments or fictitious permissions.

## Evidence and remaining work

Exact native Host tests use real LibJS, actual worker-to-owner delivery and real GC,
with a controlled OS provider. Exact macOS provider tests intercept native peers
before use and never invoke the original preferences/post/permission methods.
Read-only checks compare native results against independent framework queries.

A separate opt-in test uses a genuine unique defaults suite (the original personal
standard-defaults factory is never called), verifies typed writes/fresh native
readback, and removes only that owned domain. It also posts one uniquely named
notification only within the probe process and proves native local delivery and
removal. It does not establish cross-process persistence, physical/distributed/
workspace delivery, personal domain mutation or packaged permission/capture.

See the [public contract](../api/system-preferences.md) and
[Electron systemPreferences](https://www.electronjs.org/docs/latest/api/system-preferences).
