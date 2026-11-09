#!/usr/bin/env python3
"""Compose exact-document permission gates without modifying pinned Ladybird files."""

import argparse
import hashlib
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PINS = {
    "Notification.cpp": (
        "Libraries/LibWeb/NotificationsAPI/Notification.cpp",
        "9cdf4003d66a9ced64c28fc1a5f8d6898c2605e0cb3731374c7678314b269fc0",
    ),
    "Permissions.cpp": (
        "Libraries/LibWeb/PermissionsAPI/Permissions.cpp",
        "914dddd5fec3635a54f3ab6cb6d9de6d9896f851304de7f564ab82430d963a7f",
    ),
    "WebContentPage.cpp": (
        "Libraries/LibWebView/WebContentPage.cpp",
        "cd63de48c088f7d0127206ea464cd073728a6c645a117539c32326c49cf97930",
    ),
    "WebContentClient.cpp": (
        "Libraries/LibWebView/WebContentClient.cpp",
        "041e568e36b42e1f16f0a36349d2f975a94befea2ca71d0c12f07d13e3507809",
    ),
}


def changes(name, original):
    if name == "Notification.cpp":
        settle = "    auto settle = GC::create_function(realm.heap(), [&realm, global, promise, deprecated_callback, settings = GC::Ref(settings)](PermissionsAPI::PermissionState) {"
        state = "        auto permission_state = get_the_notifications_permission_state(settings);"
        return '#include "permissions/Hooks.h"\n', [
            (
                settle,
                settle.replace(
                    "(PermissionsAPI::PermissionState)",
                    "(PermissionsAPI::PermissionState firefly_decision)",
                ),
            ),
            (
                state,
                "        // Embedder request decisions are independent from synchronous query policy.\n        auto permission_state = Firefly::RendererPermissions::active()\n            ? firefly_decision : get_the_notifications_permission_state(settings);",
            ),
        ]
    if name == "Permissions.cpp":
        return '#include "permissions/Hooks.h"\n', [
            (
                "    auto current_state = permission_state(descriptor, settings);",
                "    auto current_state = Firefly::RendererPermissions::request_state(descriptor, settings);",
            ),
            (
                "        if (settings->top_level_origin->is_opaque() && answer != Web::Permissions::EffectiveState::Prompt)",
                "        if (!Firefly::RendererPermissions::active() && settings->top_level_origin->is_opaque() && answer != Web::Permissions::EffectiveState::Prompt)",
            ),
            (
                "    // 5. Let key be the result of generating a permission key for descriptor with settings's top-level origin and settings's origin.\n",
                "    if (auto decision = Firefly::RendererPermissions::query(descriptor, settings_object); decision.has_value())\n        return *decision;\n\n    // 5. Let key be the result of generating a permission key for descriptor with settings's top-level origin and settings's origin.\n",
            ),
        ]
    if name == "WebContentPage.cpp":
        start = "    if (!is_file) {\n        if (auto state = manager.effective_state(*permission_name, *top_level_origin);"
        end = "    // A page asks for notifications only from a gesture,"
        if original.count(start) != 1 or original.count(end) != 1:
            raise ValueError("Review permission manager request bypass seam")
        begin = original.index(start)
        finish = original.index(end, begin)
        cached = original[begin:finish]
        use = "    if (!permission_reaches_document(name, *document, *top_level_origin))\n        return false;\n"
        request = "    auto generation_when_asked = manager.decision_generation(*permission_name, *top_level_origin);\n"
        return '#include "native/permissions/Adapter.h"\n', [
            (
                cached,
                "    // Preserve ordinary browser decisions when no embedder owns this session.\n    if (!Firefly::Permissions::active()) {\n"
                + textwrap.indent(cached.rstrip(), "    ")
                + "\n    }\n\n",
            ),
            (
                use,
                use
                + "    if (Firefly::Permissions::active())\n        return Firefly::Permissions::may_use(const_cast<WebContentPage&>(*this), document_id, name);\n",
            ),
            (
                request,
                "    if (Firefly::Permissions::active()) {\n        Firefly::Permissions::dispatch_request(*this, request_id, document_id, *permission_name);\n        return;\n    }\n\n"
                + request,
            ),
        ]
    if name == "WebContentClient.cpp":
        seam = (
            "bool WebContentClient::page_may_capture_audio(WebContentPage const& page) const\n{\n"
        )
        return '#include "native/permissions/Hooks.h"\n', [
            (
                seam,
                seam
                + "    // Media grants are explicitly unavailable in this initial embedder slice.\n    if (Firefly::Permissions::active())\n        return false;\n",
            )
        ]
    raise ValueError("Unknown permission translation unit")


def adapt(name, original):
    prefix, replacements = changes(name, original)
    output = original
    for old, new in replacements:
        if output.count(old) != 1:
            raise ValueError(f"Review missing/ambiguous permission seam in {name}")
        output = output.replace(old, new)
    restored = output
    for old, new in reversed(replacements):
        restored = restored.replace(new, old)
    if restored != original:
        raise ValueError("Permission composition altered unrelated upstream bytes")
    return prefix + output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    for name, (relative, digest) in PINS.items():
        source = (ROOT / "vendor/ladybird" / relative).read_bytes()
        if hashlib.sha256(source).hexdigest() != digest:
            raise SystemExit(f"Review permission adapter after Ladybird update: {relative}")
        original = source.decode()
        output = adapt(name, original)
        prefix, replacements = changes(name, original)
        for old, _ in replacements:
            for invalid in (original.replace(old, ""), original + old):
                try:
                    adapt(name, invalid)
                except ValueError:
                    pass
                else:
                    raise AssertionError("Permission seam drift accepted")
        args.output_dir.mkdir(parents=True, exist_ok=True)
        target = args.output_dir / name
        if not target.exists() or target.read_text() != output:
            target.write_text(output)
    print(
        "PASS permission source pins, query/request/use hooks, exact unrelated bytes and seam drift rejection"
    )


if __name__ == "__main__":
    main()
