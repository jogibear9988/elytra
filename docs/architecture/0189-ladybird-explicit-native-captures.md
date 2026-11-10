# ADR 0189: Candidate explicit native capture ownership and ICU resources

Status: standalone candidate validated; full Firefly/update acceptance remains pending.

Ladybird cdcb9a replaces conservative GC closure scans with explicitly typed
captures. Native runtime functions now use GC::captures and captureless callables.
Direct JS values/cells are traced directly. Counted native services expose their
actual realm/callback edges. No blanket raw-pointer capture exemption or hidden
JS value in an opaque native lambda is introduced.

Borrowed native owners need a separate revocable lifetime anchor. A strict syntax
pass alone missed collection during shutdown: EventLoop destroyed ImmediateQueue
storage before a forced collection visited its installed callback. Native owners
now revoke before member teardown; visitors check the independent anchor before
dereferencing native memory. Calls that can occur after retirement use try_get
and explicit rejection. Runtime-only get asserts the internal admitted-call
invariant. SharedCapture owns its service and visits only declared live JS edges.
An embedded-host regression retains a real GC::Function across destruction and
collects while live, inside the revoked destructor, and after deletion.

The candidate's no-JIT configuration also exposes a JIT-only Rust method without
its feature gate. An exact owned source composition adds only cfg(feature=jit),
retains Cargo manifests/build scripts/versions, and routes other modules back to
upstream. Explicit paths preserve Rust nested-module directories. The native
compiler wrapper delegates cache/other-crate invocations unchanged and fingerprints
source selection; drift fails before output. Keep warnings fatal and JIT disabled.
Remove this narrow workaround when upstream fixes the original declaration.

ICU now ships data externally and the browser trims converters that LibWeb does
not need. Mite intentionally uses ICU's Node-compatible CJK mappings, so our build
selects the upstream full-data copy branch. Stage the real data with hashes beside
installed executables/bundles; relocation must not rely on SDK paths or environment
variables. Linux runtime layout copies use physical hardlinks to avoid duplicating
payload bytes. Full Firefly helper/dylib relocation remains a separate gate.

The update also exposed quadratic JSON IPC framing in Debug mode: a 768KiB frame
missed its unchanged child watchdog. Bounded geometric accumulation with single-pass
native delimiter scans and strict decoding preserves limits and EOF/reentry behavior
while avoiding repeated concatenation/rescans. Real fork and controlled fragmented/
quota/Unicode cases pass. This improves the existing process channel; it does not
claim zero-copy renderer file transfer or complete VS Code compatibility.

All 141 Debug tests in the isolated candidate worktree pass, including the extra
Hub source-fidelity guard; the standalone checkpoint registers the 140-suite
subset. Native CJK decoding passes after tar/ZIP/app-layout relocation. Vendor
source stays unmodified. New Hub/RemoteView bootstrap and renderer security,
platform, GUI and release gates remain unfinished before accepting this pin on master.
