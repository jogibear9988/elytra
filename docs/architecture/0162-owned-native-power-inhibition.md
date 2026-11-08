# Owned native power inhibition

Status: implemented; genuine macOS assertion lifecycle passes locally. Controlled
component validation is recorded in the public contract and progress log. This extends
powerSaveBlocker, separate from passive powerMonitor observations. Native
Windows/Linux providers are unavailable; no physical sleep guarantee is claimed.

## Decision

Use real macOS IOPM NoIdleSleep/NoDisplaySleep assertions. A host-owned provider
keeps a bounded logical request registry and one effective assertion. Display
inhibition takes precedence; stopping the last display request restores any
remaining app-suspension request. Zero is a valid initial request ID. IDs increase
per host and are never reused. Import and unknown-ID queries do not touch the OS.

Acquire a replacement before releasing previous protection. Native creation or
release failure rejects the operation and preserves logical requests. If rollback
release also fails, retain that owned ID and retry it before another mutation;
never accumulate an unbounded set of unowned assertions. Last stop and explicit
application shutdown release only assertions created by this provider. Shutdown
revokes logical admission permanently; failed OS cleanup gets bounded destructor
retries. It cannot promise successful OS release after a persistent native failure.

Use synchronous IOPM calls in this first slice so native failures are observable
rather than returning fictional success. This differs from Electron's asynchronous
wake-lock backend and can briefly block the owner thread. Native data/ownership is
independent of the VM; no GC finalizer or OS callback controls request retirement.
Mite stays independent, and Ladybird integration needs no upstream modifications.

## Acceptance boundaries

Controlled peers exercise public policy, the exact native dispatcher, actual
provider IOPM argument mapping, precedence, transaction rollback, limits and
teardown without original OS assertion calls. A separate opt-in native lifecycle
probe creates short-lived assertions and reads back this process's native records,
then releases them. Neither scope establishes behavior during actual idle sleep,
forced sleep, lid close, thermal emergency or platform power policy overrides.

See the [public contract](../api/power-save-blocker.md) for current evidence/errors,
[Electron API](https://www.electronjs.org/docs/latest/api/power-save-blocker) and
[Apple IOPMAssertionCreateWithName](https://developer.apple.com/documentation/iokit/1557134-iopmassertioncreatewithname).
