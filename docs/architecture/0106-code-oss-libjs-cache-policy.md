# 0106: Explicitly disable Code-OSS's optional V8 cache on LibJS

## Decision

Code-OSS main.ts statically imports nodeCompileCache before importing mainImpl.
The cache module itself statically imports Node cache APIs and node:v8, even when
its own development/missing-cache paths return false. Mite does not supply a V8
engine, disk bytecode cache or V8 heap statistics.

Extend the audited application overlay with an explicit --libjs-cache option.
Redirect only the cache module's two engine-specific imports to the owned
TypeScript module elytraCompileCache. Preserve the upstream cache algorithms and
all other source bytes. Add guards at enable, readiness and wait entry points:
V8 cache generation, measurement and readiness waits throw specific errors before
filesystem effects. This is an application adaptation, not a node:v8 shim or a
claim that Mite implements Node compile-cache APIs.

For ordinary startup, keep Code-OSS's real development/missing branches. When a
matching cache directory exists, supply its documented disabled status with an
explicit LibJS reason and no active directory. The original warning/logging branch
then runs. Never publish a .ready marker, empty statistics or a successful cache
generation result. LibJS bytecode serialization remains separate future work.

## Compiler and diagnostic boundary

The build-only compiler uses existing pinned esbuild 0.25.12 with Code-OSS's
legacy decorator/class-field settings and ES2024 target. It verifies overlay
input/output hashes before compilation, retains real imports, and records each
compiled input/output hash. All transformations finish before output is created;
output must be fresh and separate from inputs. Preserve source and Elytra notices.
This is explicitly a diagnostic source tree: no type checking, package installation,
workbench asset build or distributable-application acceptance is implied.

The real pinned full source compiles into 11,534 JS/JSON diagnostic files locally.
Under standalone Mite, actual main.ts completes its cache setup and reaches the
real mainImpl import, which fails on original-fs. Keep that probe nonzero and
record the real error/cache status. original-fs exists only in the Firefly host;
this is a host boundary, not a new claim that raw filesystem support is absent.
Desktop host exports/options and transitive startup compatibility remain required.

## Validation

The checked-in exact upstream cache/static dependency corpus uses actual Code-OSS
modules, not replacement path/environment services. Compare original Node and
adapted Node/Mite for development/missing paths. Exercise the adapted disabled
branch, once-only logging/kind mapping, and explicit unsupported generation,
measurements, flush/statistics/wait errors. Require no false cache kind/active
directory/ready marker/statistics. Check original preservation and compiler refusal
of overlay drift before output creation. Keep node:v8 and Node cache APIs absent
from Mite's builtin inventory/namespace.

console.warn now writes through the existing native stderr formatter, as required
by the actual upstream disabled branch. Node-style object inspection/substitutions
remain absent. This exposed event-target-shim's duplicate-listener advisories;
filter only its W06/W07 in the owned adapter, matching Node's silent duplicate
semantics. Unchanged lifetime/abort regression tests must remain green.

Local macOS Release passes all 72 suites and a non-GUI Firefly integration build.
New cache-slice native Linux/Windows acceptance remains pending. No Ladybird edits.
