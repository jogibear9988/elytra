# 0069: An explicit shared OpenSSL provider for Firefly

Status: implemented as an opt-in local macOS ARM64 build/package mode; hosted
acceptance and other platforms remain open. Defaults stay OFF.

## Problem and decision

Mite and the renderer already share the engine in a Firefly app, but direct static
OpenSSL links copy crypto code into multiple native binaries. The current complete
package audit finds repeated external definitions in Firefly, WebWorker, LibCrypto
and LibTLS. Removing an explicit dependency and relying on whichever symbols an
engine library happens to export would introduce an unstable ABI boundary.

Use the exact pinned static SSL/Crypto archives to build one private app dylib,
libfirefly-openssl.dylib. CMake's WHOLE_ARCHIVE linkage deliberately retains its
complete input API. Preserve the OpenSSL includes/compile requirements, keep its
imported targets unchanged and redirect consumer link/interface properties in the
owned deferred renderer graph. Include ordinary and visible imported targets;
rewrite both named dependencies and exact archive paths. Exclude the provider and
original OpenSSL imports to avoid cycles. No upstream source change or new crypto
implementation is involved. The independent standalone Mite graph stays static.

Gate the experiment to a shared Firefly runtime graph on macOS ARM64 and OpenSSL
major ABI 3, with actual pinned .a archive inputs. Other configurations explicitly
reject. This is not a general dynamic-vcpkg policy, system OpenSSL dependency or
standalone CLI redistribution change. Keep SYSTEM header treatment to avoid
changing warnings/recompiling the engine merely because the provider is owned.

## Distribution and evidence

The normal app packager stages and relocates the provider beside the other physical
dylibs and signs it under the existing policy. BUILD.json records sharedOpenSSL.
In enabled mode, audit both apps against the pinned archive inventory before writing
hash manifests. Require complete external definitions in exactly that provider and
explicit main/engine dependencies. Record input hashes and direct consumers.
Exclude Mach-O private externals consistently instead of allowing named exceptions.
Default mode must omit a stale provider left in a reused build directory; dependency
checks still reject stale enabled binaries if they were not rebuilt after switching.

Native tests exercise a SHA-256 known answer and real in-memory verified TLS 1.2/
1.3 handshakes/encrypted binary delivery, with a wrong-hostname negative control.
Existing actual Firefly crypto/main, renderer/compositor, isolated-world, diagnostic
helper and production-preload gates also pass. Real Designer edit/Monaco/HTML/native
window controls and relocated package/hash/signature gates protect the full app.
The public provider audit rejects the original duplicated package as a negative
control. This does not add Mite TLS/fetch or establish full security review.

The recorded same-profile package comparison saves 6,823,162 bytes (6.51 MiB),
including provider cost and signatures. App/resource/notice sizes are unchanged;
full figures and configuration limits are in the [size report](../development/package-size.md).
Do not extrapolate symbol counts into bytes or claim runtime performance/memory
improvements without measurements. Native hosted M1 package acceptance remains
required before changing defaults or publishing an alpha.
