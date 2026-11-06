# ADR 0059: explicit C++ size optimization profile

Status: local Mite profile tested; full framework size and native acceptance pending.

C++ LTO increased the measured local framework size (package-size.md), so it
remains optional and off by default. The next experiment uses size-focused C++
compilation rather than removing features, resources, helpers or license notices.

An owned CMake helper applies -Oz (or /O1 for MSVC-compatible drivers) after the
upstream Release options to all existing project C++/Objective-C++ object targets.
Configuration checks compiler support and rejects non-Release builds. No upstream
source or vcpkg package profile is edited. Rust keeps its existing Release profile.

The renderer and release drivers expose --optimize-size independently of --lto.
Both default off. A generated-command guard rejects an absent/overridden final
size flag rather than assuming setting CMAKE_CXX_FLAGS_RELEASE defeats upstream's
later -O3. Package BUILD.json records each project's selected flag and enabled
setting. All ordinary release gates remain required, including relocated Designer
operation and security tests. Combining flags requires its own native validation.

Local macOS size compilation uncovered the null-deallocation SDK issue fixed by
ADR 0058. Mite then passes 38 tests; the initial failing build is retained as
negative evidence. The renderer rebuild remains live and must be reconfigured to
include that adapter before final acceptance. No size or speed benefit is claimed
for the complete framework until actual stripped package measurement. Windows /
Linux profile configurations have not been certified by the macOS tests.
