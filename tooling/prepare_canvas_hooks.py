#!/usr/bin/env python3
"""Compose native canvas defaults at the pinned actual painting and surface-clear seams."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREFIX = '#include "canvas/Canvas.h"\n'
ENTRIES = {
    "Document.cpp": (
        "Libraries/LibWeb/DOM/Document.cpp",
        "aa141eea4268596154906a47bf72d4ba85d2e8c8e94706dddbac0a6be1c318c3",
        "    return CSS::SystemColor::canvas(canvas_color_scheme(read)).blend(background_color(read));\n",
        "    return Firefly::Canvas::base(*this, CSS::SystemColor::canvas(canvas_color_scheme(read))).blend(background_color(read));\n",
    ),
    "PaintingRustBridge.cpp": (
        "Libraries/LibWeb/Painting/PaintingRustBridge.cpp",
        "b26dbd622878e07e06d66012097ae73cd96d8e93feb6636e5682274725ddacfa",
        "        inputs.canvas_color = CSS::SystemColor::canvas(color_scheme);\n",
        "        inputs.canvas_color = Firefly::Canvas::base(document, CSS::SystemColor::canvas(color_scheme));\n",
    ),
}


def adapt(source, seam, hook):
    if source.count(seam) != 1 or PREFIX in source or hook in source:
        raise ValueError("Review native canvas paint seam")
    return PREFIX + source.replace(seam, hook)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    outputs = {}
    for name, (relative, digest, seam, hook) in ENTRIES.items():
        path = ROOT / "vendor/ladybird" / relative
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise SystemExit("Review pinned native canvas seam: " + relative)
        original = path.read_text()
        output = adapt(original, seam, hook)
        assert output.removeprefix(PREFIX).replace(hook, seam) == original
        for invalid in ("", original.replace(seam, ""), original + seam, output):
            try:
                adapt(invalid, seam, hook)
            except ValueError:
                pass
            else:
                raise AssertionError("Missing/duplicate canvas paint seam accepted")
        outputs[name] = output
    args.output.mkdir(parents=True, exist_ok=True)
    for name, output in outputs.items():
        path = args.output / name
        if not path.exists() or path.read_text() != output:
            path.write_text(output)
    print("PASS native canvas fingerprints, byte preservation and missing/duplicate seam controls")


if __name__ == "__main__":
    main()
