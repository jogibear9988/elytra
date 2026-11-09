#!/usr/bin/env python3
"""Compose the owned per-view canvas fallback into exact pinned LibWebView source."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "Libraries/LibWebView/ViewImplementation.cpp"
DIGEST = "a0f06e8804478c1a6faa3fac266f7f74acdfd0c280a626ee1dbbddbe60899f85"
PREFIX = '#include "native/canvas/UI.h"\n'
NEEDLE = "Gfx::Color ViewImplementation::preferred_canvas_background_color() const\n{\n"
REPLACEMENT = (
    NEEDLE
    + "    if (auto color = Firefly::CanvasUI::color(*this); color.has_value())\n        return *color;\n"
)


def adapt(original):
    if original.count(NEEDLE) != 1 or PREFIX in original:
        raise ValueError("Review native canvas UI seam after upstream change")
    output = original.replace(NEEDLE, REPLACEMENT)
    if output.replace(REPLACEMENT, NEEDLE) != original:
        raise ValueError("Canvas UI adapter changed unrelated source bytes")
    return PREFIX + output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    data = (ROOT / "vendor/ladybird" / SOURCE).read_bytes()
    if hashlib.sha256(data).hexdigest() != DIGEST:
        raise SystemExit(f"Review canvas UI source pin after Ladybird update: {SOURCE}")
    original = data.decode()
    output = adapt(original)
    for invalid in (original.replace(NEEDLE, ""), original + NEEDLE, PREFIX + original):
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Canvas UI adapter accepted missing/duplicate/repeated seam")
    if output.removeprefix(PREFIX).replace(REPLACEMENT, NEEDLE) != original:
        raise AssertionError("Canvas UI adapter lost original bytes")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    target = args.output_dir / "ViewImplementation.cpp"
    if not target.exists() or target.read_bytes() != output.encode():
        target.write_bytes(output.encode())
    print("PASS canvas UI pin, exact source reversal and seam drift/repetition refusal")


if __name__ == "__main__":
    main()
