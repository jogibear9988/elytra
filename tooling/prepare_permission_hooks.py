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
        "42c244647a76d7fe03142e94d23b46aa44efac0ac550d6bee52f3b3a8fadcab1",
    ),
    "Permissions.cpp": (
        "Libraries/LibWeb/PermissionsAPI/Permissions.cpp",
        "7886260b072eb3db5f7a995d4eaf7087f4cb7205eadd6e3441512162ede2ac55",
    ),
    "WebContentPage.cpp": (
        "Libraries/LibWebView/WebContentPage.cpp",
        "ec5fed5d57c4aab14cc2bee6ef1b7a20cbd03c189b7e8c89e1663747a89f3ee2",
    ),
    "WebContentClient.cpp": (
        "Libraries/LibWebView/WebContentClient.cpp",
        "ba9a9443631ed388fc42cc705dff640795e50d50ce4152ffa0369e4dcea3c104",
    ),
}


def changes(name, original):
    if name == "Notification.cpp":
        # Keep upstream's explicit traced capture list and queued promise settlement.
        # Only the embedder decision differs from the browser permission-store read.
        settle = "    auto settle = GC::create_function(realm.heap(), GC::captures(realm, global, promise, deprecated_callback, GC::Ref(settings)), [](auto& realm, auto& global, auto& promise, auto& deprecated_callback, auto& settings, PermissionsAPI::PermissionState) {"
        state = "        auto permission_state = get_the_notifications_permission_state(settings);"
        return '#include "permissions/Hooks.h"\n', [
            (
                settle,
                settle.replace(
                    "PermissionsAPI::PermissionState)",
                    "PermissionsAPI::PermissionState firefly_decision)",
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
                "    if (*permission_name == Web::Permissions::PermissionName::Microphone && view().microphone_authorization() == MicrophoneAuthorization::Denied) {\n",
                "    if (*permission_name == Web::Permissions::PermissionName::Microphone && view().microphone_authorization() == MicrophoneAuthorization::Denied) {\n"
                + "        // Preserve OS denial without entering browser permission UI.\n        if (Firefly::Permissions::active()) {\n            deny();\n            return;\n        }\n",
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
        if old == new:
            raise ValueError(f"Permission seam replacement made no change in {name}")
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
        if name == "Notification.cpp":
            assert output.count("PermissionsAPI::PermissionState firefly_decision)") == 1
            assert (
                "? firefly_decision : get_the_notifications_permission_state(settings);" in output
            )
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
