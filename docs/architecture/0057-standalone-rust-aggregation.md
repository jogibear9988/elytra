# ADR 0057: one Rust dependency graph for standalone Mite

Status: implemented and tested locally on macOS; native Windows validation pending.

## Problem

With Ladybird 97ad425b5125f20c516e1a8d3b5bb02f391fc975, the standalone C++ library
graph links independently built Rust staticlibs. These include common Unicode,
regex and standard-library dependencies with potentially different feature sets.
Native Windows run 42127377539 failed at linking with duplicate public FFI and
crate initializer definitions. macOS had duplicate-symbol warnings. Ignoring
multiple definitions can select inconsistent implementations and is unacceptable.

## Decision

An owned link-only Rust crate depends on the five upstream Rust roots: LibJS,
LibURL, LibUnicode, LibRegex and LibTextCodec. Cargo resolves their features once
and produces a single static archive. Owned CMake redirects the imported archive
locations to that archive while retaining the existing upstream C++ initializers,
native system-library requirements and FFI-header generation. The original archive
builds remain dependencies for header generation; optimizing that build work is
separate from selecting the correct linked implementation.

Use upstream's Cargo environment/setup helpers to retain allocator integration,
SDK/compiler flags and LibJS JIT/sanitizer feature selection. The owned lock is
derived from the pinned upstream lock; a configure/CTest guard rejects package
name/version/source/checksum entries outside that inventory. Cargo builds with
--locked. Updating Ladybird includes reviewing and regenerating this lock.

Remove upstream's static ELF allow-multiple-definition options in this owned
standalone graph. Do not add a Windows force-link override. Ladybird is unchanged.
Firefly's shared LibWeb/LibJS graph is unaffected; this is not a second engine or
a new dependency on Firefly in Mite.

## Validation and limits

Local macOS Release compilation and strict symbol resolution succeed. Apple ld
can still warn that the same aggregate archive path occurs repeatedly through
different imported targets; this is not a duplicate symbol definition. The CLI's
dynamic dependencies contain no LibWeb/Qt. The local unstripped CLI increases
32,080 bytes, so this change makes no size-reduction claim. The package-size guide
keeps Firefly's separate OpenSSL consolidation and LTO opportunities explicit.

Native Windows linking and behavioral tests require a subsequent hosted job.
Linux, JIT and sanitizer configurations are not certified by the macOS Release
gate. Do not infer working Designer/Firefly Windows support from standalone Mite.
