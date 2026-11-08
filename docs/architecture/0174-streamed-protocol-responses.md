# ADR 0174: Bounded asynchronous protocol responses

## Decision

Extend the existing native resource service with asynchronous response producers.
Keep the descriptor-backed file API and its exact wire schema intact. A separate
stream-response schema carries status, ordered byte-valued headers, optional length,
HEAD and null-body state. Neither schema exports a path or native capability to the
renderer. Reuse the authenticated byte channel, request IDs, epoch, fixed deadline,
document quota and process-wide admission leases.

A producer receives at most 64 KiB of actual consumer credit and has one outstanding
pull. Its completion is owner-thread-only and checks request identity, pull generation,
the original pull maximum, current credit, total size and declared length. Unknown
lengths finish only on explicit EOF. Stage completed chunks until the channel is
writable; an asynchronous callback must not bypass transport backpressure. Retire
native state before cancellation callbacks can reenter. HEAD and null-body responses
never pull and still cancel an unread producer. Ordinary EOF releases the producer
without reporting an abort. Preserve admission leases throughout the entire body.

The renderer accepts both exact schemas and builds real LibWeb responses and body
channels. Upstream CORS checks and forbidden-header filtering still apply. Unknown
lengths remain unknown; completion must match the actual received offset. The
existing navigation size limit also bounds unknown-length bodies. Custom status
phrases are not carried by this internal wire format.

A private maintained ESM controller uses captured genuine Request/Response and
stream primitives. Handler and reader cancellation share an owned AbortController;
late handlers cannot revive retired generations. It reads only on explicit demand,
copies bounded chunks, and rejects oversized source chunks rather than accumulating
an unbounded response. It does not change standalone Mite fetch.

## Acceptance and remaining integration

Native probes exercise metadata and real separate-process resource channels.
LibWeb probes exercise actual responses, binary streams, EOF, GC and cancellation;
the two-process JavaScript fetch probe consumes modern streamed responses too.
Node/Mite differential tests cover the private response controller. These checks
require no visible window or desktop interaction.

This is transport groundwork, not public protocol.handle support. Default-session
registry dispatch, VM/native producer bindings, builtin file interception, and main
net.fetch dispatch still require integration and public acceptance. In particular,
file interception must run before native file loading for every owned page and
navigation, without granting file an ordinary custom origin or filesystem access.
Code-OSS still cannot display a verified workbench or open/edit/save a workspace.
