# 0063: A fixed Apple ARM64 CPU baseline for releases

Status: implemented in the owned build drivers; hosted package acceptance pending.

## Evidence

Hosted macOS release run 42125911196 at 004b7fe failed while compiling
webrtcclient_service. The generated Cargo command used -Ctarget-cpu=native;
ring 0.17.14 rejected its static ARM capability set in two const assertions in
cpu/arm/darwin.rs. The assertions require NEON, AES/PMULL and SHA-256 features.
The log establishes the incompatible feature set, but does not identify exactly
which features the virtual runner reported or establish a defect in its hardware.
Do not disable the assertions, suppress warnings or patch the crypto crate.

## Decision

For macOS ARM64 distribution builds, use Ladybird's existing
ENABLE_CI_BASELINE_CPU option. At the pinned revision it selects -mcpu=apple-m1
for project C++ and -Ctarget-cpu=apple-m1 for Rust. This avoids deriving a
distributed binary's instruction set from the build machine. Preserve -Dwarnings,
crate versions, license notices and all normal runtime/package gates.

The release driver enables the option for both standalone Mite and Firefly.
The renderer driver exposes --baseline-cpu for local reproduction and rejects it
outside the currently checked macOS ARM64 platform. Ordinary local renderer builds
retain their native CPU default; all driver settings are explicit to prevent stale
cache values selecting an unintended profile.

tooling/check_macos_cpu.py checks the generated C++ command inventory and every
generated ARM64 Cargo invocation, not just a requested CMake option. Fail if
commands are missing, use native/another CPU, or lose the fatal Rust warning flag.
BUILD.json records each project's cpuBaseline switch. Prebuilt dependencies and
Objective-C/Objective-C++ compilation keep their existing upstream/toolchain
configuration. This is not a certificate of every dependency's minimum CPU/OS.

## Validation and limits

With Rust 1.98.0, an independent locked/offline ring 0.17.14 probe reproduces both
const assertion failures using the generic ARM64 CPU. With apple-m1 it compiles
and runs the SHA-256 abc known-answer check. This supports the feature-baseline
decision; it does not claim that the generic CPU exactly reproduces the hosted
runner's native feature detection.

Check both real generated graphs and rebuild/test standalone Mite locally. Record
those results in PROGRESS. Full Firefly/Designer packaging on the hosted runner
still has to pass before publishing an alpha or reporting a new package size.
Ladybird and all Rust dependencies remain unmodified.
