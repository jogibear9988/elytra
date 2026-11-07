# ADR 0103: Declare and verify a single packaged DBus provider

Status: implemented; the compiled native component passes at cff79fb in
[run 42377484491](https://github.com/jogibear9988/elytra/actions/runs/42377484491).
Complete application/archive and extracted Designer acceptance now also pass at
197cd2a in [run 42379262065](https://github.com/jogibear9988/elytra/actions/runs/42379262065).
This remains an experimental Ubuntu 24.04 x64 X11 candidate, not broad distribution support.

## Evidence and decision

Native Linux renderer run 42374318919 at 66cbe5c passes the renderer/helper/byte
handoff/application stages, then archive staging rejects two different
libdbus-1.so.3 contents. SDK LibWebView explicitly links pkg-config DBus; independent
helper/plugin closures can resolve the distribution provider. Both original
locations can run correctly while a flat package lib directory cannot retain two
different contents under the same name. Keep that conflict guard.

Declare one policy for libdbus-1.so.3: use the provider the built frontend actually
resolves with its original RUNPATH and a clean LD environment. Do not guess a
directory, add DBus to the external host ABI list, pick the first dependency seen
or rename incompatible libraries to hide a collision. No upstream source or build
output is changed. The application packager owns this policy; the generic ELF
stager still refuses undeclared different-content collisions.

## Loader ABI checks and provenance

A selected provider must already occur in an inspected original closure and have
the exact declared SONAME. Before copying dependencies or patching ELF objects,
use readelf's dynamic symbol
tables to identify exports of the original candidate providers and all matching
undefined imports in the selected executables, plugins and DSOs. Require exact
symbol versions and compatible symbol kinds. An unversioned import requires a
default/unversioned export; a non-default version alone is insufficient. Refuse a
policy with no verifiable consumer imports, unknown/host providers or missing ABI.

Record every original candidate path/hash, selected provider/hash and each
consumer's required symbols in BUILD.json providerOverrides. The normal library
inventory records the selected source and final packaged hash; notices follow that
actual source. Copy one physical provider, retain content deduplication, relative
RUNPATHs and relocated closure checks. Unknown collisions continue to fail with
both paths/hashes to support the next review.

## Acceptance and limits

The component fixture compiles independent executables with different valid
versioned providers of one SONAME. A compatible declared provider must serve both
after relocation and removal of original build paths, with one physical DSO and
unchanged sources. An incompatible version, implicit collision or unobserved/host
selection must fail before mutation. Pure parser/unsupported-host checks do not
establish Linux execution.

Matching symbols/versions/types is not full semantic or C structure-layout proof.
DBus's selected provider is already the frontend's native baseline; the complete
archive, live loader provenance and actual relocated application/Designer gates
remain required. Do not generalize this rule to arbitrary library replacement or
advertise native package support from the component fixture alone.

Symbol/version interpretation follows the primary
[GNU readelf reference](https://sourceware.org/binutils/docs/binutils/readelf.html)
and [linker version script contract](https://sourceware.org/binutils/docs/ld/VERSION.html).
