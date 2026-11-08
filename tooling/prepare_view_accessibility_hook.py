#!/usr/bin/env python3
"""Keep native Cocoa accessibility behind its actual Qt platform, preserving pinned bytes."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "WebContentViewAccessibility.mm": "36276d40d050da2782c3e996dddedea042f0353b08e2fa8c42d0f9e9b6e4f37e",
    "WebContentViewAccessibility.h": "be7fd0c403de815a4d01034770a6fe9a7645927236adc340a872f812f6ed1671",
}
INCLUDE = '#include "WebContentViewAccessibility.h"\n'
EXTRA_INCLUDE = "#include <QGuiApplication>\n"
ENTRYPOINTS = [
    ("void install_accessibility(WebContentView* view)\n{", "return;"),
    (
        "void update_accessibility_tree(WebContentView* view, bool announce_load, bool report_focus)\n{",
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
        validate(name, (source / name).read_bytes())
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
