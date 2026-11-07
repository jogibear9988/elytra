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
original message IDs remain unchanged. Build the ordinary and owned helpers and
run check-firefly-byte-renderer: raw delivery must pass, while unregistered/zero
cookie/duplicate handoffs close safely without breaking the native document
control connection. Keep the product receiver disabled until its authorization
and public routing gates exist. These adapters never write to the vendor tree.

## File module seam

Review tooling/prepare_file_module_hook.py and renderer-bridge/loading when changing
Fetching.cpp. The guard checks the complete pinned fingerprint, one condition and
byte preservation; do not merely refresh the hash. Verify the native policy default
is off outside the product helper, then run check-firefly-isolation and check-firefly.
The latter runs actual external-module/MIME/CSP/file-security comparisons and the
IPC fixtures with directly loaded ESM, without the earlier inline workaround.


## Custom protocol origin seams

The renderer graph also composes LibURL URL::origin and LibWebCommon's
potentially-trustworthy-origin decision through
[prepare_protocol_origin_hooks.py](../../tooling/prepare_protocol_origin_hooks.py).
Review both fingerprints, the exact early-return seams and CMake source ownership
on an upstream update. The native-only immutable policy defaults empty; standalone
Mite does not use it. Run check-firefly-protocol-origins against actual native
LibURL/LibWebCommon, separately from the standalone source-composition guard.
This does not certify custom-standard parsing or request/resource transport; see
[ADR 0111](../architecture/0111-custom-protocol-origins.md).
