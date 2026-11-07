# Pinned Code-OSS compatibility source

The manifest pins Microsoft's unmodified performance.ts and MIT license at
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
