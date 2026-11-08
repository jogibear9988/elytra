# Owned native notification generations

Status: implemented; real OS authorization/delivery remains unaccepted. The
production JS policy, native dispatcher and exact macOS provider state pass with
controlled peers. A bare-executable native identity preflight passes read-only.

## Decision

Use the macOS UserNotifications system framework for actual local notification
submission and responses. Objective-C++ handles OS data/authorization/categories
without a VM or Qt objects. C++ marshals plain token/event records to the owner
thread; maintained ESM exposes Electron-style Notification/EventEmitter APIs.
Unsupported platforms and properties fail explicitly. Mite remains independent.

Native support checks are read-only: OS, eligible application bundle identity and
delegate ownership. A bare CLI returns false. Only show may request authorization;
true support does not establish signing validity, permission or a visible banner.
Construction only owns validated JS options. Show acknowledgement means the native
request was accepted, not that OS notification policy displayed a banner. Managed
programmatic close is synchronous in Firefly, a documented lifecycle difference.

## State and ownership

Each show receives a fresh monotonic token. Detach the old generation before OS
cleanup or user callbacks; a close listener's reentrant show/close/exit takes
precedence over the outer operation. Mutable properties affect the next snapshot,
not a native request already submitted. Bound active generations and native JSON/
UTF-8 options. Close/exit revokes callback authority before native teardown.

The native dispatcher queues worker events through an owner QObject, then the
existing busy-VM/job delivery path. No native callback holds JS/realm/GC roots.
Posting and receiver teardown share a mutex; pending IDs and notifications are
owner-only. Retire terminal tokens before provider close. Stop clears ownership
under the lock, then invokes provider teardown outside it so synchronous native
callbacks cannot deadlock or reach a destroyed receiver/VM.

The provider coalesces authorization, rechecks canceled generations before opening
an authorization request, and retains actual OS callback slots until completion.
Close removes pending/delivered IDs; a late add completion removes its old ID again.
Already displayed OS UI cannot be synchronously rolled back. Request identifiers
and action categories include provider UUID/generation identity, preventing late
old submissions from acquiring new button labels. Read/merge/write category
transactions are serialized and preserve foreign categories. Never replace a
foreign UNUserNotificationCenter delegate; release only our own delegate.

## Acceptance boundaries

JS policy tests exercise validation, snapshots, quota, failures, event payloads and
reentry with controlled native records. The exact production dispatcher runs with
controlled worker completions across close, stop, receiver deletion and VM teardown.
The exact macOS provider runs against test-only Objective-C system peers: singleton
factory implementations are intercepted before any provider call and never invoked;
actual immutable notification value objects remain in use. Tests control settings,
authorization, submission, responses and late callbacks, including category and
slot ownership. This proves provider state behavior, not OS authorization/delivery.

No default test calls Electron Notification.isSupported: that implementation can
lazily request authorization. Signed-bundle OS delivery, OS/user dismissal/actions,
Focus-mode behavior, persistent history, restart activation, icons/replies and native
Linux/Windows providers require separate implementation/acceptance.

References: [public contract](../api/notification.md),
[Electron Notification](https://www.electronjs.org/docs/latest/api/notification),
[Apple UserNotifications](https://developer.apple.com/documentation/usernotifications).
