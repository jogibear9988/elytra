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
   Bootstrap uses the indexed gitlink and verifies the manifest; run it after
   staging the candidate.
4. Configure a fresh review build using
   `cmake --preset dev -B build/upstream-review`, then
   `cmake --build build/upstream-review --target mite --parallel 4`.
   Adapt our CMake graph and host code to API changes. LibJS/LibWeb integration
   APIs are not a stable ABI; an update may require such work.
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
7. Commit the pin, our necessary adapter changes, validation evidence, dependency
   changes and docs/PROGRESS.md together. Record remaining regressions honestly.
   A rejected candidate stays unmerged; restore both manifest and gitlink to the
   previous accepted commit together, then initialize the submodule again.

The update workflow is documented and its local guard is tested. No newer
Ladybird candidate was evaluated as part of adding this workflow. Native
Windows/Linux upgrade validation, CI automation and full renderer upgrade
validation remain outstanding. The Rust archive duplicate-symbol warnings in ADR 0004 remain open.
