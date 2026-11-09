# Updating Ladybird without a downstream fork

Ladybird must remain an unmodified upstream submodule. Put compatibility changes
in our runtime, renderer adapters and build configuration. Prefer upstream fixes
for missing engine/platform functionality. Do not maintain a private patch stack
or edit vendor/ladybird to make our integration build. If an upstream change
blocks one slice, record the blocker and continue independent work.

The indexed gitlink and vendor/ladybird.json must name the same full commit.
The manifest supplies the generated process.versions.libjs value. Historical
ADRs retain the revisions they inspected; they are not active build pins.
Configure, every Mite build and CTest check that the submodule checkout matches
the pin and contains no staged, unstaged or untracked source changes. These are
local integrity checks, not cryptographic proof of upstream provenance. Obtain
candidate commits from the official repository in .gitmodules / the manifest.

## Review one candidate

1. Start with a clean source tree and a passing baseline. Fetch the official
   upstream remote (`git -C vendor/ladybird fetch origin`) and inspect a specific
   full commit, its changes and release/build notes. Do not follow a moving
   branch implicitly or update production pins automatically.
2. Check out that commit detached inside vendor/ladybird. Set the same revision
   in vendor/ladybird.json and stage both paths with
   `git add vendor/ladybird vendor/ladybird.json`. The guard uses the index so a
   candidate can be built before committing it.
3. Inspect upstream rust-toolchain.toml, vcpkg.json, Meta/CMake, library target
   dependencies, licenses and platform support. Update our toolchain/dependency
   pins and README requirements if necessary. Keep upstream source untouched.
   The standalone mite/rust/Cargo.lock must retain only package versions/sources/
   checksums from the candidate's Cargo.lock, plus our aggregate crate. Regenerate
   it from a copy of that upstream lock with cargo tree --manifest-path
   mite/rust/Cargo.toml, then run python3 tooling/check_mite_rust_lock.py. Review
   the five root crates/features and RustAggregate.cmake against upstream's Cargo
   helpers and imported targets (ADR 0057); do not resolve independent upgrades.
   Bootstrap uses the indexed gitlink and verifies the manifest; run it after
   staging the candidate.
4. Configure a fresh review build using
   `cmake --preset dev -B build/upstream-review`, then
   `cmake --build build/upstream-review --target mite --parallel 4`.
   Adapt our CMake graph and host code to API changes. LibJS/LibWeb integration
   APIs are not a stable ABI; an update may require such work.
   Review the owned ExecutionContextAdapter.cpp against the upstream deallocation
   body and Rust destroy contract (ADR 0058). Its exact-call/TU guards require
   review if upstream changes or fixes null handling. Retain the null/moved-owner
   native regression and remove the adapter when upstream makes it unnecessary;
   never alter upstream to satisfy our hook guard. For size builds, verify the
   generated-command guard still finds the selected optimization flag last.
5. Install the locked npm fixtures as documented in README, then run
   `ctest --test-dir build/upstream-review --output-on-failure` and the CLI
   examples. Inspect compiler/linker warnings, runtime library dependencies,
   licenses and process.versions.libjs. Compare behavior to the documented Node
   reference; do not relax tests merely to accept a new engine revision.
6. Run native Windows/macOS/Linux checks as runners become available. Record
   unavailable checks as unverified. Renderer/IPC/security and packaging smoke
   tests must join this gate as those subsystems are implemented; current Mite
   tests cannot validate future Firefly behavior.
   On macOS, also build/run check-firefly-isolation and
   check-firefly-renderer-helper in the renderer graph described in
   renderer-build.md. Review our Main.cpp/SessionAdapter.cpp compile seams against
   the candidate's WebContent main.cpp/WebDriverBiDi.cpp, including included
   declarations, sandbox order and archive extraction. The token guard rejects
   missing/duplicate names, not arbitrary semantic changes. Preserve the unchanged
   helper negative control and before-script/frame cleanup assertions. Also review
   HostEnqueuePromiseJob/HostCallJobCallback and direct async-resume paths against
   our WorldJobGuard: preserve script/callback contexts and microtask ordering,
   ensure new job types cannot bypass revocation, and retain bound/proxy/await/
   async-generator/late-resolution/GC controls, including owner release/detach
   during active jobs and target continuations after source revocation. Its owned-world dispatch uses public
   SDK APIs; no engine patch is acceptable to restore these gates. Review the
   rejection tracker/error reporter too: owned reasons/Promises must not enter
   principal page events, including foreign-Promise/source-origin cases, while page
   Reject/Handle controls must continue to pass. CallbackAdapter.cpp substitutes
   the WebIDL AbstractOperations.cpp TU in our LibWeb graph; review both callback
   overloads, included declarations (including PerformanceObserver), source-list/
   generated-file handling and Report/Rethrow/Promise conversion behavior. Preserve
   the exactly-one-TU/count guards and queueMicrotask error/lifetime tests. Run the
   ordinary renderer and Firefly/Electron gates too, since they share this library.
   TimerMapAdapter.cpp also substitutes WindowOrWorkerGlobalScope.cpp, forwarding
   the original map/GC edges while observing set/remove/clear. Review its header
   declaration, map access patterns, interval rearming and native start/stop order.
   Keep exact TU/token guards, pre-snapshot GC/native stop assertions, and the
   source-detach test with an existing page interval still running. Do not replace
   targeted release with clearing every timer in the document.
   ObserverAdapter.cpp additionally substitutes DOM/MutationObserver.cpp. Review
   its original class declaration, weak node-list append/iteration/clear accesses,
   node/transient registrations and disconnect/queued-record behavior. Preserve
   exactly-one-TU and three-token guards. Review mutation delivery settings/realm,
   record Array construction and native wrapping against WorldJobGuard's source
   argument rewrapping. Retain poisoned page-prototype, receiver/identity, GC,
   repeated observe/disconnect/re-observe and live page observer controls. Never
   substitute blanket document teardown for targeted observer release.
   ListenerAdapter.cpp substitutes EventTarget.cpp to wrap the free native
   invoke_event_listener entry before currentTarget/Event wrappers are created.
   Preserve its preincluded BindingsGlue declarations and exactly-one-TU/token
   guard. Review CallbackType::associated_realm origin rules, EventDispatcher's
   work before invocation and exception reporting after it, and ensure inactive
   sources cannot reach wrapper creation or handleEvent lookup. Keep function/
   object/getter/proxy, receiver/cancelation, last-owner/context/GC and live-page
   error controls. Generic WebIDL operations and event handlers are separate paths.
   The same listener TU observes the original event_listener_list_changed calls
   and maps the returned abort algorithm ID without replacing its closure. Keep
   all virtual declarations preincluded (EventTarget, WebSocket, XMLHttpRequest,
   RustEventTarget), two notification tokens and one abort token. Review original
   add/remove/once/capture ordering, duplicate signal registrations, Optional::map
   and AbortSignal's aborted flag/iteration/clear order. Preserve exact signal-ID
   removal, in-flight late registration and native-GC evidence with rooted signals;
   page listener/signal controls must continue to pass.
   EventTarget's listener/handler free definitions now pass through
   tooling/adapt_event_entries.py, which renames definitions only so internal
   handler calls still hit the guard. The same generator now adapts only declared
   handler storage/trace/scope/realm expressions (ADR 0040). Review
   all three exact signatures, generated #line/includes/dependencies and the source
   preservation test. Keep exact listener/handler/reporter tokens and verify internal calls.
   Retain page-created-listener/source-replacement, error/strict-bool and native
   beforeunload conversion tests: cancellation precedes ToString; nonempty
   returnValue skips it; detach/last-owner release must wait through conversion.
   Review all four handler-map addresses, native visit_edges, storage-owner realm
   selection and the principal scopes for inline attributes/full erasure. Keep
   target/realm metadata weak and trace values through native target liveness.
   Retain page/two-world getter/identity/order, native erasure/rearm, inline string,
   detached-cycle GC, foreign callback and source-detach/page continuity controls.
   The preservation test reverses every declared edit to recover exact upstream
   bytes; counts cannot substitute for this semantic review.
   CustomEventAdapter.cpp/CustomEventBindingAdapter.cpp additionally substitute
   native DOM/CustomEvent.cpp and generated Bindings/CustomEvent.cpp. Review the
   constructor provenance, successful init assignment after dispatch checks,
   visit_edges and generated detail getter return/realm. Preserve original bindgen
   dependencies, exactly-one source guards and the custom-event preservation/drift
   test. Run same-world/page controls, frozen graph/cache/reset tests, reader-realm
   TypeErrors with zero traps, foreign-prototype provenance, writer GC/revocation,
   target cache release and actual helper/fresh iframe checks. Native payload
   retention and other event fields remain explicit limits (ADR 0041).
   ErrorEventAdapter.cpp/ErrorEventBindingAdapter.cpp add native ErrorEvent
   registration/tracing and generated constructor/getter routing. Both event JS
   constructors now record writer realms explicitly; native constructors must not
   inherit an ambient Source caller. Review the EventTarget special onerror fifth
   argument seam too: it must use the callback realm's EventValues reader. Keep
   exact native/generated counts, bindgen dependencies and preservation tests.
   Retain getter/argument identity, reverse page input, zero-trap/function denial,
   synthetic-reader error stack, native page error under Source dispatch and all
   previous CustomEvent/resource controls (ADR 0042).
   MessageEventAdapter.cpp/MessageEventBindingAdapter.cpp add native data-cache
   tracing, successful legacy-init invalidation, explicit constructor provenance
   and the generated data getter. Preserve postMessage serialization/deserialization
   in the principal receiver realm, task delivery and source/ports rewrapping/cache
   algorithms. Keep exact source/bindgen dependencies and preservation tests. Run
   snapshot/GC/init metadata controls plus actual two-way postMessage, function
   DataCloneError and source-detach/queued-publication tests (ADR 0043). Transferred
   ports and broader message/sender policies remain unverified.
   Review principal context suspension/restoration at listener/handler, non-Promise
   Report callback and event exception-reporting entries (ADR 0044). VM saved stacks
   must remain GC roots while native Source owner frames remain active. Preserve
   page Error.stack privacy, page Promise/timer survival after Source detach, ordinary
   onerror, foreign principal beforeunload conversion and Source last-owner/GC reentry
   controls. Other WebIDL exception/Promise modes remain explicitly outside this gate.
   NodeFilterAdapter.cpp substitutes DOM/NodeFilter.cpp with definition-only
   accept_node renaming. Preserve its header declaration, internal public calls,
   exact generated dependency/source-count and byte-preservation/drift tests.
   Review wrapping before call_user_object_operation, getter/proxy/function receiver
   semantics, callback result ToInt32 and traversal exception/active-flag propagation.
   Keep revocation before wrapping/lookup, last-owner/GC through result conversion,
   principal stack/job controls and helper iframe removal during conversion. The
   cross-world .filter getter remains a separate open boundary (ADR 0045).
   NodeIteratorBindingAdapter.cpp/TreeWalkerBindingAdapter.cpp route exactly their
   generated filter getter returns (ADR 0046). Preserve bindgen dependencies, both
   one-TU counts and native/generated reverse-preservation/drift tests. Review the
   NodeFilter constructor notification's active writer realm independently of
   CallbackType settings/associated realm; keep provenance weak, propagate guard
   layers and mark revocation before weak writer expiry. Preserve same-world/null,
   page/second-world getter denial, foreign prototype/callback, zero traps, writer-GC
   and weak TreeWalker/filter collection controls plus native invocation tests.
   Review Document's iterator registration/GC edges separately; filter metadata
   cleanup is not proof of upstream native container reclamation.
   Standalone CTest now also includes the node:os differential gate (ADR 0047).
   Preserve builtin CJS/ESM identities and native OS/environment observations when
   adapting LibJS string/value/array APIs. Its libuv adapter needs no Ladybird seam
   or new dependency. Run the Firefly ESM/HTTP fixture's OS imports too, since the
   main process uses the shared Mite runtime graph; native Windows/Linux remain
   independently unverified.
   The additional mite-os-data gate checks native CPU/account copies (ADR 0048),
   GC-rooted result graphs and raw bytes passed to existing Buffer encodings. Keep
   libuv allocation/free ownership, option getter/error identity, named-export
   identity and fresh graph/Buffer behavior when adapting LibJS allocation APIs.
   CPU speed/counters are live observations; do not relax genuine shape/monotonic/
   provenance regressions to get an upgrade through. Run the Firefly main-process
   CPU/account ESM imports too. No vendored engine changes are needed.
   Run mite-os-network and the standard-C++ mite-os-netmask probe (ADR 0049).
   Retain native interface allocation/free ownership, fresh GC-rooted grouped
   results, decoded own-property names and address/mask/scope/CIDR semantics when
   adapting LibJS APIs. Keep independent mask/address checks; samples are separate
   observations, so investigate actual OS reconfiguration before diagnosing a
   differential failure. Firefly's main ESM fixture also calls this API. No new
   Ladybird source seam or vendor change is needed.
   Retain mite-os-priority validation/native syscall checks (ADR 0050), including
   safe finite/int32 conversion, own-child setter mutations and native error fields.
   Preserve explicit reference controls for negative-zero Node assertions and
   argument-formatter property effects; they are documented differences, not a
   reason to reproduce an engine abort. The Firefly main fixture confirms its own
   existing priority. Windows mapping/permissions still need native validation.
   Run mite-os-constants too (ADR 0051). Keep native header/libuv numeric values,
   availability guards and constant/group descriptor differences when adapting
   LibJS object construction/integrity APIs. Preserve null prototypes, immutable
   numbers, mutable group identity, native signal freezing and CJS/named ESM export
   identity. Reference tests compare the available host set; native Windows/Linux
   still require their own runs. The Firefly main fixture checks the named export.
   Run mite-crypto-random and the Firefly main-process random ESM fixture (ADR 0052).
   Preserve rooted VM output, native-only worker storage, owner-thread completion,
   prompt native-byte release and EventLoop pending-work/shutdown ownership. Keep
   zero-size synchronous callback and nonzero Promise-order controls, buffer/UUID
   identities, explicit missing APIs and documented receiver/cache differences.
   Do not replace uv_random with a JS PRNG to accommodate an SDK change. Provider
   failures/extreme allocations and native Windows/Linux still need validation.
   Run mite-crypto-hash and Firefly's main ESM hash control too (ADR 0053).
   Preserve checked EVP return values and RAII context ownership; do not patch
   LibCrypto assertions downstream. Retain copy independence, finalization,
   native byte-view bounds, SHAKE length and Transform/pipeline controls, including
   the isolated Node negative-zero/object-mode assertion reference cases.
   The existing pinned OpenSSL target is linked explicitly by our runtime graph.
   Provider failures, full stream/error parity and Windows/Linux remain unverified.
   Revalidate the pinned Designer app and release bundle after engine/helper changes.
   tooling/tests/test_designer.py must exercise the real app's document edit and Monaco,
   not merely compilation or loadURL. Keep packaged helpers/resources physical,
   loader paths relative, helper entitlements/sandbox intact and exact dependency
   notices/pins in release manifests. Re-run relocated native app gates; do not
   substitute a different engine or skip required platforms to make a release green.
7. Commit the pin, our necessary adapter changes, validation evidence, dependency
   changes and docs/PROGRESS.md together. Record remaining regressions honestly.
   A rejected candidate stays unmerged; restore both manifest and gitlink to the
   previous accepted commit together, then initialize the submodule again.

The non-Windows Firefly build also substitutes one owned LibWebView ProcessMonitor
translation unit (ADR 0062). Run tooling/check_process_monitor.py and review both
upstream source/header hashes when updating. Do not restore waitpid(-1) or a
competing SIGCHLD handler: application children belong to libuv, renderer/service
helpers to the registered-PID monitor. Revalidate native child completion and
registered renderer/helper crash detection together. Remove the adapter if an
upstream selective-monitoring API supplies the required ownership boundary.

The internal ContextBridge publisher in
[ADR 0067](../architecture/0067-atomic-context-api-publication.md) also depends on
native function Realm metadata, execution-context enumeration and own-property
descriptor semantics. Revalidate atomic nested publication, trap-free rollback,
foreign caller/call/apply reentry denial, detach/Promise revocation and the
compiled helper's first-script/srcdoc controls. Keep default data snapshots
function-free; publication tokens are not an IPC decoder. Passing these controls
does not enable public preload selection or production helper/IPC integration.

The update workflow is documented and its local guard is tested. No newer
Ladybird candidate was evaluated as part of adding this workflow. Native
Windows/Linux upgrade validation, CI automation and full renderer upgrade
validation remain outstanding. ADR 0057 addresses standalone Rust duplicate symbols
locally; native Windows/Linux and the separate renderer graph remain independent gates.

Public preloads also use the owned Qt preparation adapter (ADR 0068).
Run tooling/prepare_view_preload_hook.py --output build/view-hook-check.cpp and
review both WebContentView.cpp/header hashes and the exact preparation seam.
The generated source retains upstream bytes/notices with one include and one call;
no vendor patch is allowed. Revalidate newly hosting page configuration before the
first author script, especially file-to-HTTP process replacement and independent
windows. Both diagnostic FireflyWebContent and production FireflyRuntimeWebContent
must build; only the latter ships. Run tooling/tests/test_public_preload.py against the
build and packaged executable, preserving explicit failure and source budgets.

The optional shared OpenSSL graph (ADR 0069) redirects named and exact-path archive
links without changing upstream targets' source. Revalidate whole-archive provider
creation, inherited header requirements, TLS 1.2/1.3/hostname and hash tests, package
symbol inventory and relocated full-app gates if OpenSSL/CMake linkage changes.
Do not keep an accidental engine export as a substitute for the explicit provider.
Unknown archive ABI/platform changes reject; static standalone Mite stays independent.

## Byte transport bootstrap

The [trusted byte bootstrap adapter](../architecture/0101-trusted-byte-endpoint-handoff.md)
also fingerprints Meta/CMake/code_generators.cmake, the original Python IPC
generator and WebContentServer.ipc. Review tooling/prepare_byte_codegen.py and
tooling/generate_byte_bootstrap.py together; do not only refresh hashes. Confirm
the targets/hsts_preload wrapper order still selects the owned generator and all
original message IDs remain unchanged. The private byte handoff stays at the first
appended ID; navigation handoff is the second and document handoff the third.
Build ordinary and owned helpers and run check-firefly-byte-renderer: raw delivery must pass, while unregistered/zero
cookie/duplicate handoffs close safely without breaking the native document
control connection. The owned navigation receiver now validates the receiving
ConnectionFromClient/PageHost and current exact native navigation; inspect the
NotRouted stub inheritance and staged LibIPC declaration on updates.

The existing WebContentPage composer also has one guarded call immediately before
async_create_navigation_params, and another before the selected host's
async_populate_navigation. Review all three event/navigation/document seams and
their exact restoration; the original request must follow the optional handoff on the same
connection only while page/navigation/worker/loader remain current. Preserve the
retained page/loader and weak-navigable check across callback reentry. Run check-firefly-resource-renderer for real production-helper custom
DOM navigation, wrong-ID/page/missing/duplicate refusal and channel retirement.
Also preserve upstream's destination document ID before population. The resource
gate loads real external ESM/CSS/binary GET/HEAD, refuses wrong/missing document
grants, preserves the original against duplicate endpoints and revokes on navigation.
Inspect exact local settings/document/page/navigable identity checks and the pending
five-second grant expiry. It opens no desktop window. General Response handlers and broader session routing remain unimplemented. These adapters never write to the vendor tree.

The normal main bridge now uses these seams through its default-session FileRoutes
service. On updates, preserve canonical root ownership across selected hosts,
initial URL/document ID binding, queued main-VM dispatch and hook teardown. Run
check-firefly-protocol-host without a window and batch check-firefly-public-protocol
only for relevant renderer/window changes (or hosted CI). The latter must load
real public callback resources and preserve failure/revocation/generation controls;
an internal native fixture alone does not accept that public path.

## File module seam

Review tooling/prepare_file_module_hook.py and renderer-bridge/loading when changing
Fetching.cpp. The guard checks the complete pinned fingerprint, the file-module
condition and the scheme-fetch dispatch seam after upstream cancellation checks.
It requires both seams exactly once and restores all original bytes after removing
the owned additions; do not merely refresh the hash. Run check-firefly-protocol-fetch
for actual JavaScript fetch/HEAD/abort and default-denial checks; verify the
native provider remains absent in product helpers. Verify the native policy default
is off outside the product helper, then run check-firefly-isolation and check-firefly.
The latter runs actual external-module/MIME/CSP/file-security comparisons and the
IPC fixtures with directly loaded ESM, without the earlier inline workaround.


## Custom-scheme CORS fetch seam

The owned prepare_file_module_hook.py composition also recognizes opted-in native
CORS schemes at main_fetch's scheme check, then dispatches the authorized file
response at http_network_or_cache_fetch. It preserves upstream CORS checks,
response filtering and CSP. Review both exact seams and the whole-source fingerprint
when updating Ladybird; retain reverse byte-preservation and ambiguous/missing-seam
refusals. Never change vendor sources to make the seam fit. The owned Response
adapter rejects unauthorized metadata before body credit but keeps that document's
channel usable. Run check-firefly-protocol-fetch and batch check-firefly-public-protocol
for real response filtering, policy denial, target revocation and reload. See
[ADR 0134](../architecture/0134-simple-custom-protocol-cors.md).

## Custom protocol origin seams

The renderer graph composes LibWebCommon's potentially-trustworthy-origin and
fetch-scheme decisions, plus the WebContent launch call, through
[prepare_protocol_origin_hooks.py](../../tooling/prepare_protocol_origin_hooks.py).
Review all three fingerprints, exact seams and CMake source ownership on upstream
updates. The shared Fetch/Infrastructure/URL.cpp predicate must add only explicitly
registered standard schemes, preserve builtin HTTP/local classification and restore
all upstream bytes. Run check-firefly-protocol-routing against the real engine and
LibWebView URL/input classifiers; this check opens no window or external application.
LibURL URL.cpp remains upstream: native and DOM consumers instead share Rust
Url::origin through [the owned Rust composition](../../tooling/rust-origin/prepare.py).
Review the pinned lib.rs/origin.rs/ffi.rs hashes, module layouts, borrowed-origin
ABI and synchronous callback ownership; do not just refresh hashes. Original
parser modules, Cargo manifests/lockfiles and notices must stay unchanged.

The native-only immutable policy defaults empty; standalone Mite does not use it.
Run check-firefly-protocol-origins against actual native LibURL/LibWebCommon,
separately from standalone source/driver guards. Run check-firefly-protocol-renderer
(or check-firefly-protocol-dom-origins) for strict actual owned-helper origin and
configuration acceptance. Both archive depfiles must include the generated Rust
root/origin/ffi and owned helper. Verify changed composition inputs invalidate Cargo
through the explicit cfg fingerprint; wrapper changes alone can leave fresh old
artifacts. Inspect existing compiler-cache chain order and native driver argument
transport on each platform. Standard parser/mutation and request/resource transport
acceptance remain separate. See [ADR 0113](../architecture/0113-shared-rust-origin-policy.md).

The owned helper Entry.cpp consumes the bounded immutable origin policy before
calling upstream main. Check argv ownership, stripping and error-before-bootstrap
behavior on updates. Parent launch seals even an empty policy; public registration
must not diverge from existing spare processes. See [ADR 0112](../architecture/0112-native-protocol-configuration.md).

## Local crash-report compositions

Review tooling/prepare_crash_hooks.py and both complete CrashReport.cpp/CrashReportStore.cpp
fingerprints before updating. Preserve exact source restoration and missing/duplicate
seam refusal. Directory selection and normal-thread metadata formatting are the only
insertions; do not change LibCore's fatal handler, raw record format, file locking,
mode/owner validation, saved-name checks or original build identity. Upstream ASCII
validation requires JSON Unicode escapes in captured application metadata. A recovered
main record must keep its original annotations; only pre-configured helper records
need the surviving parent's frozen global metadata fallback. Run the public crash
reporter gate on relevant changes/hosted CI: real owned main/helper signals, recovery,
PID ancestry, private modes and annotation separation must pass. An imported module
or hand-written report fixture does not accept native capture.

The native application IPC handoff in
[ADR 0141](../architecture/0141-native-preload-byte-channel-binding.md) appends one
NotRouted control message to the owned generated endpoint, independently of file
resource grants. Keep all upstream IDs and previous owned message IDs unchanged.
The generator checks that exactly four owned methods are appended and that native
callback forwarding uses the selected connection stub. Bootstrap consumers must
include the canonical staged LibIPC/FireflyBootstrap.h declaration, not both the
source and staged copies. Revalidate channel/hello unit gates and the production
helper's reload/multiwindow handshake before accepting an upstream update.

Trusted publication getters in [ADR 0143](../architecture/0143-trusted-publication-getters.md)
add an explicit native reader to the existing snapshot engine. Revalidate ordinary
own descriptors, getter function class/realm metadata, rooted key vectors and alias
values across getter-triggered GC. Keep default snapshot paths getter-free and
verify the publication collision/reentry/revocation controls and exact call budget.

The [sandbox process metadata adapter](../architecture/0144-sandbox-process-metadata.md)
captures the helper executable before upstream sandbox initialization and constructs
only document-local data in the product isolated realm. Preserve native PID/execPath,
exact argument/environment schema budgets, own data properties (including __proto__)
and unsupported-method export blocking. Re-run the native isolation gate and the
focused production-helper process gate on relevant upstream changes. Do not add
application arguments to the upstream helper command line or install process in the
principal page/diagnostic realm. The manifest supplies both reported engine hashes.

[Preload browser zoom](../architecture/0145-native-preload-web-frame-zoom.md) uses the
existing page-client browser zoom callback and parent view acknowledgement. Preserve
that path's native viewport/input/compositor scaling and revalidate factor/level
conversion, document liveness after nested event-loop work and raw capability
publication refusal. Run the native isolation and focused preload zoom gates.

The [file-input adapter](../architecture/0146-bounded-native-file-input.md) attaches
LibWebView's on_request_file_picker and returns the existing basename/byte SelectedFile
variant. Revalidate request-page routing, cancelled/obsolete dialog completion,
regular-file/count/byte limits, FileList creation and the actual helper File.text
roundtrip. Do not replace names with absolute paths or infer File provenance from
JavaScript-visible fields. Interactive platform sheets remain a separate acceptance.

The [native File provenance registry](../architecture/0147-native-file-provenance-registry.md)
uses a fingerprinted HTMLInputElement composition with one FileList root and one
post-creation notification. Preserve the native File/document/selected-span identity
checks and GC-safe weak entries/tombstones. Re-run check-firefly-isolation and the
focused native file-input regression. Main-to-helper path delivery and the public
webUtils API are still missing; this observer does not accept either. Do not add
File/DOM values to general IPC or infer paths from script-visible properties.

[Chooser request ownership](../architecture/0148-native-file-request-ownership.md)
adds checked Page request/close/exact-target delivery seams. Revalidate original
multiple mode, monotonic serials, weak target/document/page identity, invalid-result
cancellation and preservation of ordinary SDK requests without a sender. The native
request scope does not itself implement the parent/helper transport or public API.

## Session permission adapters

Review the owned permission compositions whenever the upstream permission model or
canonical document lifecycle changes. `tooling/prepare_permission_hooks.py` pins
Permissions.cpp, Notification.cpp, WebContentPage.cpp and WebContentClient.cpp; the existing native
IPC adapter composes its page hook rather than replacing it. Preserve both the
query-before-cache path and the independent permission-request admission path.
Custom outcomes must not become origin-wide grants or survive their document and
policy generation. Keep upstream trust, Permissions Policy, activation and OS
backend gates, and preserve ordinary unconfigured Ladybird behavior.

The private bootstrap generator must preserve original endpoint IDs and bind its
checks to the actual sending client/page/document. Review the guarded helper-launch
snapshot and ordered policy updates, including helpers hosting existing subframes.
A source digest or unique needle proves identity of a seam, not semantic safety.

After the renderer rebuild, run `check-firefly-session-permissions` and
`check-firefly-permission-boundary` where the explicit offscreen frontend exists.
These use real owned documents and denied permission requests; the boundary driver
also tests the native grant model without invoking a device provider. They add no
production test API and should not produce an OS permission prompt. Fresh platform
and actual device/backend acceptance remain separate requirements.

## Session request adapters

Review Fetching.cpp compositions in prepare_file_module_hook.py, the private
FetchController cancellation composition in prepare_web_request_controller.py and
WebSocket containment in prepare_web_request_websocket.py. Preserve exact pinned
fingerprints, upstream byte reversal and missing/duplicate-seam rejection. The
request gate belongs after upstream CSP/HSTS checks and before transport; the HTTP
head gate must precede CORS, while owned protocol/file heads must wait before CORS
and body credit. Preserve the original transfer lease and controller stop steps.

The helper launch policy is composed by prepare_protocol_origin_hooks.py and
consumed before the VM starts. Preserve cookie/PID binding, typed stage flags,
canonical document/navigation ownership, bounded pending/queue state and both
callback and timeout revocation. Worker policy reads use the atomic snapshot;
worker jobs must not touch owner-thread controller or pending vectors. WebSocket
handshakes currently fail closed with active interception because upstream bypasses
Fetch. Keep that containment until actual handshake interception is implemented.

Run check-firefly-web-request-native and the batched exact-object offscreen
check-firefly-web-request component gate; compare the shared hidden Electron
fixture on relevant behavioral changes. Recheck existing protocol, permission and
frame gates for Fetch/ownership changes. A green schema probe alone does not prove
endpoint-zero cancellation or header changes before CORS. See
[ADR 0181](../architecture/0181-session-request-interception.md).

## Canvas background adapters

Review prepare_canvas_ui.py and prepare_canvas_hooks.py against the complete
ViewImplementation.cpp, Document.cpp and PaintingRustBridge.cpp fingerprints.
Preserve exact upstream byte reversal and missing/duplicate seam rejection.
Native UI fallback alone does not establish a colored renderer canvas. Keep
document blending, recording inputs, authored CSS precedence and ordinary iframe
behavior aligned. Never remove upstream final from WebContentView to subclass it.

Metadata-only canvas records travel through the existing trusted BiDi state
channel. Keep strict context/generation/RGB bounds, atomic snapshots, removal
before preload evaluation and the independent preload budget. No source string
is evaluated for these records. UI destruction must erase the captured opaque
base pointer without dereferencing it after base destruction.

Run check-firefly-background-color with exact application objects and the static
offscreen plugin. A pixel fixture must show its window virtually to produce
frames. On macOS, QWidget.grab reads Qt chrome rather than the separate page
CALayer; use the public native CPU tab-preview path after accepted compositor
swaps. This verifies actual canvas bytes, not physical GPU/CALayer presentation.
Recheck preload isolation, frame identity, permissions and protocol regressions
for session-state/painting changes. See [ADR 0183](../architecture/0183-native-canvas-backgrounds.md).

## Native Cocoa first-click integration

The first-mouse adapter uses the actual Qt QNSView runtime class and its original
acceptsFirstMouse: implementation without changing upstream sources. On Qt or
native view changes, re-run check-firefly-window-input: hidden Cocoa callback
checks must cover independent policies, restoration and reparenting. Verify the
runtime subclass assumptions and original input-transparency checks; an offscreen
public constructor test cannot prove native click-through behavior. See
[ADR 0184](../architecture/0184-native-first-mouse-policy.md).

The first-mouse subclass must preserve both actual runtime identity and the
reported Objective-C class from Qt/KVO. Its gate reads real AppKit safeAreaInsets
and QWindow safeAreaMargins while hidden, before/after native resize and actual
view recreation. Recheck these computed-property paths on Qt/AppKit updates;
acceptsFirstMouse callbacks alone do not validate safe rendering/presentation.
