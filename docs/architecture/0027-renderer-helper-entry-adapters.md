# 0027: Attach an early world without modifying Ladybird

Status: macOS ARM64 internal helper prototype and headless gate implemented.
Public preload, contextBridge and renderer IPC remain unsupported.

## Decision

Keep the upstream checkout unchanged. Build a separate WebContent executable
using the complete original bootstrap and BiDi service translation units via
our Main.cpp and SessionAdapter.cpp wrappers. Adapt only ladybird_main,
initialize_main_thread_vm and set_webdriver_bidi_session_state token names.
Initialize our session immediately after the real VM initialization, before
sandbox/IPC/document creation. Preserve upstream sandbox code and helper signing.

Preinclude service/class declarations before token adaptation so IPC/virtual
member declarations remain identical across translation units. Our direct BiDi
object provides the original service functions, preventing extraction of the
corresponding member from the static service archive. This depends on the pinned
build graph; it is not symbol interposition or a supported upstream embedding ABI.
Configure/build guards require exactly one occurrence per adapted token; source
review, compilation and runtime tests remain essential for every upstream update.

Own an internal BiDi session and EventSink to receive synchronous realm-created
events. Forward the originally requested event names and navigation notifications
to the existing PageClient route, preserving the original service state setter.
Internal realm events are not forwarded to the UI unless requested. No external
automation listener or general evaluation endpoint is added. This instrumentation
forces BiDi active and widens internal realm-event observation; automation-visible
semantics and complete subscription/filter compatibility have not been audited.
The production app continues to use the original helper.

For each local active document, create a rooted IsolatedWorld before page code.
Run only a compiled harmless script with a private global/prototype marker and
shared window.name ordering marker. No user file, Mite/Node binding or privileged
function enters the world. Limit realm identifiers to 1024 bytes and live worlds
to 128; initialization errors terminate the helper rather than falling back to
page-world execution. These prototype limits are not a public API contract.

Detach on realm-destroyed events, session shutdown and inactive documents. A sweep
before creation and every 10 ms covers replaced initial documents that do not
emit the expected destruction event. The document root permits safe activity
checks. Inactive cached documents lose this prototype world; BFCache restore,
cross-origin navigation, async callbacks and generation-based revocation require
separate implementation before privileged preload can be enabled.

## Native evidence and limits

The helper runs in a real separate sandboxed process. Its PID appears in both
the UI probe report and world lifecycle logs. The unchanged helper fails the
first-script marker check, while our helper passes. Top-level and srcdoc first
scripts see the ordering marker without private globals/prototypes. Seventy
create/navigate/remove frame cycles produce at least 141 world creations and 140
releases, leaving only the top-level world live. The original event-only cleanup
produced just 71 releases; the inactive-document sweep fixes this measured leak.

Resources must be physical files inside the prototype bundle: symlink staging
produced sandbox font metadata failures. Other helper binaries reuse the upstream
development bundle. Existing entitlements/signing and sandbox setup are retained;
strict codesign verification passes. Nonfatal Metal sandbox and late compositor/
temporary-profile shutdown diagnostics remain. No sandbox security audit or
distributable package is claimed. Resource overlay staging assumes a clean build
directory when evaluating an upstream update.

The gate runs headlessly and does not close the locked-desktop screenshot gate.
Windows/Linux, nondefault COI, public preload, structured-copy/function/Promise
membranes, callback revocation, bounded authenticated IPC and malicious-input
testing remain outstanding. No main-world closure or arbitrary BiDi evaluation
may substitute for those boundaries.
