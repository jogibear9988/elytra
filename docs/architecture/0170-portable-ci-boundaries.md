# Repair native portability and test ownership boundaries

## Decision

Raw duplex child IPC on Windows must request UV_NONBLOCK_PIPE for descriptor 3.
libuv's Windows stdio-pipe construction otherwise creates a synchronous child handle
when ipc is false. A pending read can serialize writes on that same duplex handle.
Keep ipc false and our existing raw framing; enabling ipc would introduce libuv's
handle-transfer framing instead. Apply this to independent Mite forks and Firefly
utility channels. Functional hosted Windows duplex acceptance is still required.

Replace our crash Reporter's POSIX-only stat admission with a small std::filesystem
policy using valid UTF-8/native paths. Require a fully absolute existing directory,
preserve directory-symlink behavior, and never create/canonicalize/change permissions.
Map only portable generic error conditions to errno; Win32 codes are not errno.
Final private-directory/descriptor checks remain the crash store's responsibility.
This does not enable upstream Windows crash capture. Ladybird stays unmodified.

## Verify actual ownership without weakening assertions

Keep synchronous collector calls while native I/O/callbacks are active. For final
reclamation, request collection from JavaScript but execute it after the embedding
driver's pump_once returns. Conservative native stack scanning must not turn stale
inactive dispatch-frame slots into apparent application ownership. Preserve both
final WeakRef-is-undefined assertions and add no production collector API.

Give test-owned event streams an owner-loop startup turn, retain target event
assertions and print actual event names on failure. Canonicalize test working
directories before comparing Node's ordinary/native realpath results. Retry only
Windows cleanup of the already bounded shell descendant after functional acceptance;
do not kill independent process trees or extend functional timeouts.

## Frame tests must use their asserted principal

Parent-authored iframe handlers cannot make positive calls directly through a
child-owned privileged bridge. Author positive operations and ordinary wrappers
inside srcdoc, wait for its actual load, and separately assert a borrowed parent
call is rejected. Prove the same child wrapper works while live and fails after
removal; attach pending-Promise rejection observation in the surviving parent.
Keep production principal and revocation checks unchanged, including cycles,
listener/handler/traversal and source-job ownership.

The local complete headless helper batch and runtime gates verify these repairs.
Fresh hosted platform results remain required; see [progress](../PROGRESS.md) and
[Windows scope](../development/windows-build.md).
