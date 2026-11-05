# 0056: Ladybird update and plain-text tooltip embedding

## Upstream review

Move the synchronized gitlink/manifest from
51ef0d743ceca25863847f8cea208c1c6f7031ce to
97ad425b5125f20c516e1a8d3b5bb02f391fc975 (2026-11-05). The new pin is 166 commits
later and includes substantial Rust CSS/storage/binding changes, renderer/compositor
changes and Qt UI text handling. Rust remains 1.98.0 and vcpkg remains
20ceb8adbf0504d4b0d5e4058a32f9db880ef25. Upstream sources stay unmodified.

Reviewed the narrow helper/callback/timer/listener/observer seams, Rust callback
FFI and source lists. Rust callbacks still delegate through the C++ WebIDL callback
entry used by our owned adapter. Token guards are only drift checks; actual runtime,
renderer, isolation and helper tests are required before accepting the pin.

## Tooltip adapter

Upstream WebContentView now calls show_tooltip/hide_tooltip from PlainText.h.
The upstream PlainText.cpp also depends on the browser's TabBar. Linking that
browser UI is inappropriate for Firefly's curated embedding target.

Provide the two entry points in our own Qt adapter with a bounded QLabel explicitly
set to Qt::PlainText. Page text cannot select Qt rich-text rendering. Limit text to
2,048 UTF-16 units, width to 480 pixels, height to 24 lines and lifetime to at most
30 seconds. Parent ownership/QPointer and generation-checked timers avoid callbacks
after owner deletion and stale timers hiding a newer tooltip. Native tests verify
literal markup, bounds and hiding. This does not reproduce every browser-tab tooltip
behavior; the optional rectangle-specific hide policy is not implemented here.

No source substitution/patch to Ladybird or sandbox weakening is introduced.

## Validation

Native Release Mite builds and all 35 CTest suites pass (19.46 seconds). Release
Firefly builds. check-firefly, check-firefly-renderer, check-firefly-isolation and
check-firefly-renderer-helper pass, including native chrome/plain text, Electron
reference states, actual LibWeb CSS/layout and owned-world revocation/error/callback
resource fixtures. The bundled Designer passes actual boot/edit/Monaco, external
stylesheet drag/no-drag geometry, minimize/maximize/restore/HTML close and absence of
Node privileges. Upstream integrity and the vendor's 48,247 tracked checkout paths
pass. Rust duplicate-symbol and local SDK deployment-target warnings remain.

Native Windows/Linux, unlocked desktop pixel presentation, optional Designer features,
complete security review and hosted package/release gates remain unverified. These
tests support this pin and their measured scope, not a claim that every upstream bug
or missing web feature is fixed. A new package regeneration and hosted macOS Release
build follow; do not publish an alpha until its actual hosted gates succeed.
