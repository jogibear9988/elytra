# ADR 0092: Order IPC sender metadata behind canonical history updates

Status: implemented; scoped native macOS acceptance passes; fresh hosted optimized
release acceptance pending.

## Problem and decision

The optimized 02ed03f hosted package run reached real desktop IPC but failed
senderFrame.url immediately after a same-document History API update. Upstream
receives the history operation before the IPC envelope, but applies that operation
on its deferred session-history traversal queue. Reading canonical URL during
initial IPC admission can therefore capture the previous URL.

Continue authenticating native routed page, top-level active document, realm and
monotonic request ID at admission. Reserve the existing bounded pending slot.
Append metadata sampling to the actual canonical traversal queue, revalidate
ownership at that position and read canonical active_document_url there. Use an
owned Qt lifetime guard so delayed callbacks cannot dereference a destroyed host.
Resolve the queue step without waiting for application JS or a handler reply;
delivery still follows the existing VM-busy/revocation checks.

Do not replace native URL authority with page/renderer claims, add guessed sleeps
or poll until a test's expected URL appears. Keep upstream source unmodified.
The native API supplies the ordering primitive; no downstream browser patch or
additional cross-process message is required.

## Acceptance and limits

The actual hidden-window security fixture passes 24 ping requests over three
fresh-document cycles, with seven immediate alternating same-document updates
per cycle and no page-side delay/retry. Existing checks retain invalid-input
refusal, zero serialization traps, raw-capability refusal, pending-request limits,
old reply revocation and window destruction. Diagnostics include ping index,
expected suffix and actual native URL on failure. Local Firefly compiles and the
scoped fixture returns success without stderr. This does not establish optimized
hosted acceptance or exhaustive history/redirect/subframe behavior. Those remain
separate required gates before a new release.
