# Pinned Code-OSS compatibility source

The manifest pins Microsoft's unmodified performance.ts, bootstrap-node.ts,
platform.ts, process.ts, nls.ts, main.ts, nodeCompileCache.ts, its static common
dependency sources and MIT license at
6531648f5ace8b77ae799262bdc97bf048461938. These files are test input, never a runtime
dependency or a substitute workbench. Preserve their bytes and license.

tooling/tests/test_perf_hooks.py verifies the recorded SHA-256 hashes, then uses
the pinned Node 24 reference's stripTypeScriptTypes in strip mode. This only erases
types; it does not bundle dependencies, replace functions or rewrite the runtime
algorithm. Both Node and Mite execute that same temporary ES module and the
authored acceptance driver. No Node process is embedded or required to run Mite
applications; Node is the existing development/test reference.

The test exercises origin-backed Code-OSS startup marks, reads, named/all clearing
and preservation of the origin marker. It does not execute mainImpl, the desktop
workbench, extension hosts or the complete Code-OSS bootstrap. Updating this source
requires an explicit revision/hash/license review and renewed behavior acceptance.

tooling/tests/test_code_oss_bootstrap.py applies the same hash check/type erasure
to bootstrap-node.ts, then evaluates the actual module under Node and Mite. It
checks SIGPIPE registration, original cwd capture/platform cwd setup, and real
portable/missing-directory/temp-path behavior using isolated temporary directories.
Source mode is explicit (VSCODE_DEV=1); no function is replaced or skipped inside
the source. Exported loader-hook/global-search/ASAR customization functions are not
exercised. This is not main.ts, nodeCompileCache, mainImpl or a desktop launch.

tooling/tests/test_code_oss_platform.py compiles the actual platform/process/nls
modules using build-only pinned esbuild 0.25.12 (const enums and type-only imports
require compilation). Compare unmodified Node, adapted Node and adapted Mite.
The declared identity adaptation lives in tooling/code_oss/overlay.py; an
unadapted Mite negative control must reproduce incorrect native classification.
Only this explicit overlay changes source; the compiler supplies no fake modules.
Input files and license are preserved and all other source bytes remain exact.
The license's optional crlfCheckoutSha256 records upstream's LICENSE.txt eol=crlf
checkout rule; the pinned fixture remains LF and integrity checked.

tooling/tests/test_code_oss_cache.py runs real nodeCompileCache/static dependencies
after compilation, comparing normal development/missing branches with original
Node and exercising the explicitly selected LibJS disabled policy under Node/Mite.
Generation/statistics/wait/flush modes must fail explicitly, without fabricated
markers or measurements. The owned helper is an application adaptation, not a
Mite builtin. main.ts is pinned for source provenance; the complete local source
diagnostic is separate from the cache component's CI acceptance.
