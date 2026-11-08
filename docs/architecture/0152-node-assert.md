# ADR 0152: Truthful portable assertions for Mite

Status: locally tested partial node:assert and real Code-OSS CLI validation.

## Context

The real Code-OSS ready/main graph passes native zlib and next imports assert.
Its argument validator uses a callable truthy assertion to reject malformed --goto
input. Real packages also need equality, errors and exception matchers. An inert
function or a source-only bypass would conceal invalid states. Reuse portable
compatibility code, while refusing known modern cases it cannot establish.

## Decision

Pin assert 2.1.0 and its entire dependency closure in the existing build lock.
Use the existing portable bundle generator and MIT/third-party notice distribution;
record every source/package/license hash. Route the provider's internal util import
to its own complete portable inspection/types implementation instead of claiming new
public node:util features. The resulting graph has no external runtime imports.
Three unique, checked build-time source seams correct the provider's legacy NaN
rules for loose scalar/deep equality, including negative assertions. Compose those
changes in memory; retain original package files and notices, and record original
source/builder/output hashes. Missing/duplicate seams fail the build. The code
executes on LibJS; Node/esbuild are build/reference tools only.

Add a small owned ESM capability policy and adapter. Cache the callable module per
loader and expose the same strict alias through node:assert/strict. Retain real
AssertionError/equality/exception behavior. Preserve missing-argument arity when
wrapping deep assertions. Reject known unsupported opaque/modern deep state, custom
tags/accessors and traversal beyond 10,000 distinct objects/depth 256. Unsupported
negative comparisons must not silently succeed. Preserve top-level identity cases.
Apply guards to exception object-pattern values before reporting success.

The provider predates modern mismatch classification: it rethrows the tested error
for a constructor/RegExp/predicate mismatch. Adapt throws/rejects to genuine
AssertionError with original actual/expected identity. A forwarding function Proxy
tracks validator exceptions without replacing constructor/instanceof checks. Propagate
validator-thrown errors, including the same actual error object, and synchronous
throws from rejects' callback unchanged. Current-realm Promise rejection validation
is tested; non-native thenables explicitly reject. Keep modern Assert/CallTracker/
partialDeepStrictEqual and deprecated fail overloads unavailable. Do not claim current
Node rich-diff/source/stack/inspection parity. See the [API contract](../api/assert.md).

## Validation and remaining work

The new mite-assert CTest runs Node/Mite differential outcomes, real Error metadata,
callable/strict/CJS/ESM identity, loose/strict/deep collection cases, cycles/symbols,
regex checks, missing arguments, sync/async errors and native-host forced GC. Separate
policy tests prove explicit rejection of known unverified values and overloads.
Pin the actual Code-OSS argvHelper.ts and verify its compiled source/output hashes;
the unchanged main/CLI validator accepts valid --goto and throws AssertionError for
invalid input on real Node and Mite. No replacement parser or assertion is used.

Local standalone/Firefly Release builds and all 84 runtime gates pass. The rebuilt
Firefly real ready/main graph now reports unsupported node:readline instead of
node:assert. No workbench/window/open-edit-save acceptance follows. Continue actual
readline demand and subsequent startup, then assets/preload/ports/File/window
integration. Full platform/extension/PTY/webview/remote requirements remain active.
Ladybird is unmodified; Mite remains independent. Fresh Linux/Windows execution and
complete modern assert parity remain unverified.
