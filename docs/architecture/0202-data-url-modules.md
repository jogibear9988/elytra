# ADR 0202: Native data-URL ES modules

Status: implemented partial Mite loader support; standalone and embedded Firefly
native macOS ARM64 validation passes, including a controlled original Code-OSS
extension workflow. Fresh Windows/Linux and full editor acceptance remain open.

## Demand

A real application resolver can return generated JavaScript as a data URL. The
original Code-OSS API resolver uses that mechanism to export extension API values
from an application-owned registry. Import-only shims or fabricated exports would
not execute its generated source or preserve the actual API object identity.

## Decision

Add an owned DataModule decoder and a data branch to the existing ModuleLoader,
before any filesystem-path conversion. Use neutral LibURL percent decoding and AK
base64, then the existing LibJS JavaScript/JSON module parsers. No renderer Fetch,
LibWebCommon, Firefly, Node or V8 dependency is introduced. Ladybird remains
unmodified. The decoder is an independent bounded implementation of the data-URL
processing rules; [Node 24.15's data URL processor](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/data_url.js)
and [format selection](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/modules/esm/get_format.js)
provide the measured compatibility baseline.

JavaScript MIME selection normalizes ASCII case and surrounding whitespace for
text/javascript and application/javascript. JSON matches application/json exactly;
Wasm's exact application/wasm essence is recognized only to refuse it explicitly.
Do not normalize those formats together: the Node reference distinguishes them.
Percent and forgiving base64 decoding feed UTF-8 source; JSON additionally strips
a leading UTF-8 BOM before native parsing. JSON type attributes remain mandatory.

Cache by the complete serialized URL. A fragment distinguishes native module
instances but is not source text; the query remains part of the body. Store real
ModuleSource metadata with import.meta.url and keep filename/dirname file-only.
Support builtins and absolute URLs from a data parent. Relative/package resolution
without a file parent fails rather than interpreting a data URL as a file or using
an implicit working directory. This is module loading, not a new sandbox.

Extend synchronous resolve-hook target/format handling through the same validated
path. Require still rejects data ES modules, including hook-returned targets.
Loader-generated CJS/builtin/host bridge access remains tied to exact native shim
record identities; a user-supplied URL or generated source cannot join those sets.

## Ownership and bounds

Root ModuleSource before compilation and retain successful modules in the normal
realm-local rooted module cache. Hooks, imports, TLA and exported values use the
existing LibJS execution/lifetime machinery. No JavaScript value is hidden in
untraced decoder state; no file-backed CommonJS cache entry is fabricated for JSON.

Admit at most 8 MiB of URL, 4 MiB decoded source, 1,024 successful data records and
32 MiB aggregate charge per loader. Charge twice URL plus twice original decoded
source bytes; preserve the BOM byte charge even when stripping it for JSON.
Cache hits and failed parses do not reserve again. The record limit prevents many
tiny URLs from defeating a text-only budget through native module metadata.
This is conservative source/identity accounting, not a maximum for the entire heap,
ASTs, exports or transient parsing allocations. Quota failures are explicit.

## Validation and remaining work

The native standalone Mite build passes all seven shared Node differential groups
and their real-GC repeat. Tests cover percent/base64 and MIME behavior, full URL
identity, JSON attributes/BOM, builtin and absolute imports, rejected relative or
package imports, resolver routing and generated API registry exports with genuine
identity and calls. Three Mite-only boundary/quota groups and the independent
1,024-record test pass. The rebuilt embedded Firefly runtime passes those data
controls, including GC and quotas. Existing standalone module, resolve-hook and
CommonJS suites remain green.

The original controlled Code-OSS extension fixture now passes activation, its
registered command, openTextDocument/showTextDocument and WorkspaceEdit/save.
Independent Python physical-file readback verifies 85 bytes. Its report records
accepted:true, native exit 0 and no cleanup signals; all 12 tracked native PIDs
are absent afterward. The existing Code-OSS helper was unchanged for the data-module
implementation; no application data-loader workaround was introduced.

This accepts the generic data-module mechanism and that controlled workflow locally.
It does not claim complete Node loader hooks, synchronous require(ESM), Wasm, new CLI URL entry syntax or full
Code-OSS extension/editor behavior, visible editor rendering, PTYs or general
native-addon compatibility. Fresh cross-platform acceptance is pending.
The public subset and exact failures are recorded in [data modules](../api/data-modules.md).
