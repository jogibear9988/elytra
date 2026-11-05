# Package size: measurements and remaining work

Size is measured from actual app files, excluding symlink aliases. Compression and
filesystem allocation are different measurements. SIZE.json records uncompressed
bundle/component bytes; the CI release report also records archive bytes. A package
with fewer bytes still has to pass native behavior, relocation and security gates.

## Measured baseline

Local macOS ARM64, source 6c3bc8f, Ladybird 51ef0d743ceca25863847f8cea208c1c6f7031ce,
Release Mite/Firefly, production dependencies, browser bundling and stripped local/
debug symbols. These are a baseline before the separate Ladybird update, not a
claim about every future build or platform.

| Output | Actual uncompressed file bytes |
| --- | ---: |
| Earlier Designer candidate | 466.2 MiB |
| Regenerated bundled Designer | 249.8 MiB |
| Firefly hello-world framework bundle | 215.7 MiB |
| Installed Electron 45.2.3 reference app, without Designer | 252.6 MiB |

The reference comparison is scoped: Electron is a development test installation,
not an equivalently configured packaged Designer. It is never shipped in Firefly.
No general size, performance or memory advantage is inferred from these figures.

## Are Mite and Firefly duplicating the engine?

The Designer app has a Firefly executable and renderer helpers, with **no separate
Mite CLI** inside that app. The independent Mite CLI is a separate release artifact.
In the Firefly build, upstream's LibJS target delegates to LibWeb; our runtime graph
explicitly selects that target instead of linking the standalone LibJS variant.

The installed .dylib version aliases are symlinks to one physical file. Helpers use
the shared libraries; the packager does not copy a full LibWeb per helper. An audit
of defined public JS/GC symbols found no overlap between the Firefly executable and
LibWeb's engine definitions, apart from the different glue each host needs.

There **is** duplicate static crypto code: Firefly and liblagom-crypto share 903
public OpenSSL symbol definitions in this baseline. This identifies a consolidation
candidate, not a byte-saving estimate. Changing linkage needs a deliberate shared
provider/build configuration and independent Mite packaging/behavior tests; relying
on accidental exported symbols from another library is not a stable API.

The updated Ladybird pin also exposed duplicate Rust dependency definitions in
the **standalone Mite** Windows link: separate static archives carry common
Unicode/regex crates. Our standalone build now links one owned aggregate Cargo
archive, retaining upstream FFI headers and initializers. This is distinct from
duplicating an entire engine in a Firefly app. The local macOS Release CLI grew
from 51,254,056 to 51,286,136 bytes before packaging/stripping (about 31 KiB), so
this fix is **not a measured size saving**. Native Windows acceptance remains
pending. See [ADR 0057](../architecture/0057-standalone-rust-aggregation.md).

The later local candidate using Ladybird
97ad425b5125f20c516e1a8d3b5bb02f391fc975 measures 249.43 MiB for Designer and
215.30 MiB for the framework. It does not include a new crypto/LTO optimization.

## What dominates the remaining framework?

| Component | Approximate baseline size / observation |
| --- | --- |
| LibWeb dylib | 54.9 MiB after stripping |
| Unicode dylib | 37.6 MiB; about 32 MiB is constant data, not debug symbols |
| All main/helper executables | 44.3 MiB |
| Dependency notices and engine resources | 23.6 MiB |
| Other libraries/signatures/metadata | Remainder of the 215.7 MiB bundle |

The link-closure audit found only one unreferenced physical dylib, wakelock, about
34 KiB. Blindly deleting libraries is therefore not a meaningful size strategy, and
link closure alone does not prove optional runtime loads are unnecessary.

## Optimization candidates

- Evaluate LTO and size-oriented native/Rust compiler settings against this measured
  baseline. The current owned renderer build explicitly disables release LTO;
  linker dead stripping is already enabled.
- Consolidate repeated static dependencies where platform/linker semantics and the
  standalone runtime permit it; preserve one consistent crypto provider.
- Consider explicit feature profiles for resources/media/PDF/RTC only with documented
  unavailable features and native tests. Do not remove helpers silently.
- Review notice inventories against actually distributed dependencies while preserving
  all required notices; current collection is deliberately broad and not a full audit.
- Continue browser-asset graph checks as optional Designer features gain coverage.
  Keep --no-bundle for investigating dynamic module paths outside measured workflows.

Unicode data cannot simply be removed while retaining the same Intl/text behavior.
Compressing downloads helps transfer size but does not reduce the installed app.
Every experiment must record build configuration, exact bytes, passed tests and
remaining functionality/platform limits. Ladybird stays unmodified.

## Repeat a measurement

```sh
python3 - <<'PY'
import sys
sys.path.insert(0, 'tooling')
from package_size import bundle_size
print(bundle_size('build/designer-verified-release/Web Component Designer.app'))
PY
```

Use otool -L for the actual native dependency graph and nm -gU for defined public
symbols on macOS. Symbol counts alone are not code size. See the
[packaging guide](../guides/packaging.md) and [release workflow](releases.md).
