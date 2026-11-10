#!/usr/bin/env python3
"""Guard native Cocoa AX while preserving the pinned RemoteView accessibility implementation.

The widget owns its AccessibilityTreeManager; HubConnection dispatches tree deltas
by RemoteView's actual view_id. Keep that binding, removed-node invalidation,
zoom conversion and native actions unchanged. Only synthetic non-Cocoa Qt handles
must stop before an NSView cast or native accessibility registration.
"""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "WebContentViewAccessibility.mm": "9f7fdd35ef3195df830eebcfc51fd8c641faf9db5b1d31b57229e54689f82411",
    "WebContentViewAccessibility.h": "aa0efb4cdfd98897d41c296bf2031c1f89dc9c64d9778a9b93ee6f30bf0ad63f",
}
INCLUDE = '#include "WebContentViewAccessibility.h"\n'
EXTRA_INCLUDE = "#include <QGuiApplication>\n"
ENTRYPOINTS = [
    ("void install_accessibility(WebContentView* view)\n{", "return;"),
    (
        "void update_accessibility_tree(WebContentView* view, bool announce_load, bool report_focus, Optional<Vector<i64>> const& removed_ids)\n{",
        "return;",
    ),
    ("void post_accessibility_focus_changed(WebContentView* view, i64 node_id)\n{", "return;"),
    (
        "void post_accessibility_announcement(String const& text, String const& live_value)\n{",
        "return;",
    ),
    ("static WebContentAccessibilityView* get_overlay(QWidget* widget)\n{", "return nil;"),
]


def guard(action):
    return (
        f'\n    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))\n        {action}\n'
    )


def validate(name, raw):
    if hashlib.sha256(raw).hexdigest() != EXPECTED[name]:
        raise ValueError("Review owned Qt Cocoa accessibility adapter: " + name)


def adapt(original):
    if EXTRA_INCLUDE in original or any(guard(action) in original for _, action in ENTRYPOINTS):
        raise ValueError("Cocoa accessibility source is already composed")
    if original.count(INCLUDE) != 1:
        raise ValueError("Review Cocoa accessibility include anchor")
    for seam, _ in ENTRYPOINTS:
        if original.count(seam) != 1:
            raise ValueError("Review Cocoa accessibility entry anchor: " + seam.splitlines()[0])
    generated = original.replace(INCLUDE, INCLUDE + EXTRA_INCLUDE)
    for seam, action in ENTRYPOINTS:
        generated = generated.replace(seam, seam + guard(action))
    restored = generated.replace(INCLUDE + EXTRA_INCLUDE, INCLUDE)
    for seam, action in ENTRYPOINTS:
        restored = restored.replace(seam + guard(action), seam)
    if restored != original:
        raise AssertionError("Cocoa adapter changed bytes outside its declared seams")
    return generated


def verify_negative_controls(original):
    for seam in [INCLUDE] + [seam for seam, _ in ENTRYPOINTS]:
        for changed in [original.replace(seam, "", 1), original + "\n" + seam]:
            try:
                adapt(changed)
            except ValueError:
                pass
            else:
                raise AssertionError("Missing/duplicate Cocoa accessibility seam accepted")
    try:
        adapt(adapt(original))
    except ValueError:
        pass
    else:
        raise AssertionError("Repeated Cocoa accessibility composition accepted")
    try:
        validate("WebContentViewAccessibility.mm", (original + "\n").encode())
    except ValueError:
        pass
    else:
        raise AssertionError("Upstream Cocoa accessibility source drift accepted")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    options = parser.parse_args()
    source = ROOT / "vendor/ladybird/UI/Qt"
    for name in EXPECTED:
        raw = (source / name).read_bytes()
        validate(name, raw)
        try:
            validate(name, raw + b"\n")
        except ValueError:
            pass
        else:
            raise AssertionError("Upstream Cocoa accessibility source/header drift accepted")
    original = (source / "WebContentViewAccessibility.mm").read_text()
    generated = adapt(original)
    verify_negative_controls(original)
    options.output.parent.mkdir(parents=True, exist_ok=True)
    if not options.output.exists() or options.output.read_text() != generated:
        options.output.write_text(generated)
    print(
        "PASS pinned Cocoa-only accessibility guards, unchanged source bytes and missing/duplicate/drift refusal"
    )


if __name__ == "__main__":
    main()
