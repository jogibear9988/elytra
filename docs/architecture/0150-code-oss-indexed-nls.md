# ADR 0150: Indexed English NLS for the Code-OSS source build

Status: accepted as a locally tested build and runtime component; no workbench.

## Context

After the explicit local desktop experiment reaches app ready, the real bootstrap
tries to load src/nls.messages.json. A source-only esbuild transform did not produce
it. An empty catalog or silencing the read error would hide a missing build step.
Upstream's build assigns stable global message indices to localize/localize2 calls
and generates matching messages, keys, metadata and an ASCII JavaScript catalog.

## Decision

Add an explicit --nls compiler option. Use the exact pinned upstream nls-analysis.ts
binding/reference analyzer and nlsMessages.ts serializer, preserving their notices
and verifying their hashes before executing build code. Keep upstream nls.ts build
pipeline as pinned reference evidence. Run their TypeScript analysis as build tools
under Node, not in Mite. Pin upstream's @typescript/typescript6 package at 6.0.2 in
our separate build-tool lock; this package's compiler reports 6.0.3. None of these
devDependencies enters the application/runtime dependency installation.

Before transpiling sorted source modules, enumerate localize then localize2 calls,
assign global indices and replace their key spans in reverse source order. Preserve
English message arguments. The actual upstream binding analyzer handles aliases,
namespace imports and locally shadowed names. Validate spans and reject overlap.
Decode only literal string/template keys/messages and key/comment metadata using
Acorn; never evaluate application expressions. Reject unsupported/dynamic forms.
Generate nls.messages.json, nls.keys.json, nls.metadata.json and nls.messages.js at
the expected src paths. Preserve translator comments and serialize the JavaScript
catalog with the upstream ASCII serializer. Record analyzer/serializer/builder
hashes, compiler package/version, counts and each generated artifact hash in the
compile report. Validate/transform everything before writing a fresh output tree;
refuse source collisions with generated assets. Without --nls the original source
compilation remains available and generates no catalog.

## Validation and limits

Local regression executes the real analyzer, tests stable call/index ordering,
aliases, namespaces, shadowing, metadata, Unicode/empty/multiline strings, safe
literal refusal, deterministic artifacts and real compiler integration. It verifies
artifact hashes and fail-before-output for dynamic messages and collisions. The
actual pinned runtime nls.ts consumes the generated indices/catalog on Node and
Mite, with identical formatted strings and localize2 original/value objects.

The full local source build produces 34,273 messages across 2,501 modules. The real
Firefly app ready bootstrap now loads this catalog without the NLS read failure and
continues to the actual unsupported node:zlib import. There is still no workbench,
translated language-pack acceptance, generated workbench asset/bundle pipeline,
source-map output or fresh Linux/Windows execution. This is not a distributable
VS Code build. Keep genuine compression/IPC, assets, preload/window integration and
open/edit/save acceptance as the next steps; full platform/extension/PTY/webview
requirements remain part of the original goal. Ladybird remains unmodified.
