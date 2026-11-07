# ADR 0113: Share native origin policy with Rust URL consumers

Status: accepted for local macOS and native Linux origin extraction (1d033fe, run 42399004586); Windows renderer acceptance remains open. Builds on [ADR 0112](0112-native-protocol-configuration.md).

## Problem

The pinned DOM URL binding calls Rust Url::origin directly. A C++ URL::origin
adapter cannot make that result agree with native consumers, even after the
correct immutable policy reaches the helper before bootstrap. Decorating the DOM
getter would leave other Rust origin extraction paths inconsistent.

## Decision

Compose one guarded entry in upstream Rust Url::origin. It asks an owned private
Rust helper for an optional tuple override, then preserves the upstream algorithm
when none exists. The helper calls the canonical C++ immutable process policy.
Reuse the existing FfiOrigin borrowed callback ABI; Rust copies the origin before
the synchronous callback returns. There is no second Rust registry or new public
FFI representation. C++ parsing/host normalization used by the callback does not
call origin(), avoiding recursion. Remove the earlier C++ URL::origin composition;
both C++ and DOM now reach the Rust origin entry.

Rust is appropriate at this existing Rust origin boundary, and C++ remains the
owner of the existing process policy and URL/host parser integration. Their small
FFI follows upstream's existing shared file-origin/registrable-domain callbacks.
Standalone Mite has neither this source composition nor the Firefly callback.

## Build ownership and freshness

Keep Ladybird, Cargo manifests, lockfiles and parser files unchanged. Fingerprint
lib.rs, origin.rs and ffi.rs. Generate only a root/module path composition and the
origin call insertion in the build directory. Preserve all other bytes and license
notices; require exact reverse restoration and refuse missing/duplicate seams or
changed module layout. ffi.rs needs explicit child paths because relocating its
module entry changes Rust's implicit child module resolution.

A native build-only compiler driver launches Python without a shell. It replaces
exactly the pinned liburl_rust root argument and verifies source/output hashes;
other crate/compiler arguments pass through unchanged. A prior compiler cache is
chained after source selection, so it receives the actual compiler inputs. The
Windows entry preserves Unicode/empty/quote/backslash arguments using native
CreateProcessW and CRT quoting. Additional workspace wrappers are refused.

The owned CMake rust_crate module includes upstream's unchanged module and adds an
unused cfg input containing the composition fingerprint to its Rust target flags.
This deliberately invalidates existing Cargo artifacts: changing RUSTC_WRAPPER
alone does not invalidate an already fresh dependency. Owned-input changes update
the fingerprint; native checks inspect actual Cargo dependency outputs for the
composed root/origin/ffi and private helper in both LibURL and LibWeb archives.
Neither a fresh-looking Cargo message nor a source-only guard is engine acceptance.

## Acceptance and limits

Require native default/configured tuple/trust tests and actual headless owned
helpers observing origins in their first ordinary page script. Cover ports,
credentials, IDNA, IPv6, relative URLs, builtin/default denial and the real getter's
brand check. Keep page/native privileges absent. Compiler routing tests cover
ambiguous/missing input, drift, cache order and native argument/exit transport;
upstream Rust-only unit tests use an empty policy because they do not link C++.

This fixes origin extraction only. It does not make the upstream non-special
custom URL parser implement Chromium standard-scheme parsing/mutation semantics,
nor add resource loading, public protocol registration, worker propagation,
fetch/CORS/CSP/ServiceWorker behavior or a Code-OSS desktop launch. macOS, Linux and
Windows evidence must remain separate.
