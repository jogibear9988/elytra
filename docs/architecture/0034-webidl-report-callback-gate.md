# 0034: Gate owned report-style WebIDL callbacks

Status: internal compile adapter and macOS ARM64 SDK/helper/app gates implemented.
Public preload/contextBridge/IPC and complete callback coverage remain unsupported.

## Decision

queueMicrotask uses HTML's microtask queue and WebIDL::invoke_callback with Report;
it does not pass through the Promise VM hooks. Adapt that common callback entry
instead of replacing a global function or copying timer/microtask bindings.

Our renderer CMake graph substitutes exactly one LibWeb translation unit with
CallbackAdapter.cpp. That file includes the complete original
WebIDL/AbstractOperations.cpp implementation under a narrow invoke_callback token
rename, then supplies its two original public overloads. No vendored source is
edited/copied, and all other implementations in that translation unit remain the
upstream code. Preinclude declarations, including the unrelated PerformanceObserver
member with the same token, to avoid changing class declarations across TUs.

Configure requires exactly one original source-list entry. Preserve source,
generated binary-directory and generator-expression entries while adding the
adapter. Configure/build token checks require four current occurrences (two
definitions, a call and a comment); compilation and behavioral tests remain the
semantic gate. This is a maintained embedding seam, not a stable upstream ABI.

An exported internal C++ gate table is thread-local and keyed by VM. It stores raw
native gate contexts, not JS objects or strong state ownership. WorldJobGuard's
VM-retained state registers/unregisters the gate and chains any previous gate.
Destroying a renderer session must not reopen queued callbacks; VM hook ownership
keeps state live until teardown. The table follows the SDK's NeverDestroyed TLS
pattern and does not add a JavaScript API, IPC endpoint or engine dependency to Mite.

Initially gate only non-Promise callback types with ExceptionBehavior::Report.
Check both the callback object's shape realm and the actual function realm,
including bound/proxy functions. Require registered owners/documents to remain
valid and enter the same rooted/deferred-detach boundaries as Promise jobs. Live
unregistered callbacks delegate unchanged. The actual upstream callback body still
performs its normal settings/callback bookkeeping, receiver and argument handling.

For owned Report callbacks, call the original entry with Rethrow, capture any
abrupt completion without inspecting its value and record a native failure count.
Return undefined on failure instead of letting the original Report path dispatch
a raw source ErrorEvent. Normal completion values retain their upstream semantics.
Unspecified/Rethrow and Promise-returning variants deliberately delegate for now;
their conversions and callback return-value boundaries need separate policy/tests.

## Evidence and limits

SDK tests check queueMicrotask order relative to promises, strict undefined receiver
and zero arguments, source/proxy/bound/foreign-target cancellation, GC, live-world
controls and an unregistered detached-world negative control. A running microtask
requests detach, releases its last external owner, finishes current DOM access,
denies a later job and then releases its native owner/physical world. A thrown
Proxy increments only the native error counter with zero source getter calls.

The actual helper now includes queueMicrotask in its removed-frame and self-removal
fixtures. Source microtask exceptions do not reach page error events, while a
normal page timer's ErrorEvent and page rejection/handled controls still work.
The SDK/helper, ordinary renderer (including compositor image/process isolation)
and actual Firefly/Electron reference gates pass. The upstream-helper control uses
the same adapted LibWeb with no registered Firefly worlds; it is a delegation
control, not a bit-identical upstream library binary.

This does not prove all timer/listener or WebIDL paths safe. Promise-returning
callbacks, call_user_object_operation, typed callback results, direct/cross-world
function transfers, cancellation of native timer/observer resources, finalization,
workers, BFCache/navigation and hard resource/queue/CPU bounds remain open. No
new public API, native Windows/Linux support, distributable package or full audit
is claimed. The separate visible-desktop gate remains open; the compositor image
gate does not replace it. Ladybird's checkout and pin remain unmodified.
