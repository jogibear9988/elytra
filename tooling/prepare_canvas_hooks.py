#!/usr/bin/env python3
"""Compose native canvas defaults at the pinned actual painting and surface-clear seams."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREFIX = '#include "canvas/Canvas.h"\n'
# Reviewed candidate changes retain both authored-background blend and opaque
# child-canvas conditions. Composition changes only the native fallback base.
ENTRIES = {
    "Document.cpp": (
        "Libraries/LibWeb/DOM/Document.cpp",
        "3c96f099784ab70d02208bc05212363f815404ed94f0ee02bb4ee1f7a1096893",
        "    return CSS::SystemColor::canvas(canvas_color_scheme(read)).blend(background_color(read));\n",
        "    return Firefly::Canvas::base(*this, CSS::SystemColor::canvas(canvas_color_scheme(read))).blend(background_color(read));\n",
    ),
    "PaintingRustBridge.cpp": (
        "Libraries/LibWeb/Painting/PaintingRustBridge.cpp",
        "c6e0a5b7fb7f7c61e33a2e51aa7265722b3aa6966373c861e9d8b4b464e6d7d6",
        "        inputs.canvas_color = CSS::SystemColor::canvas(color_scheme);\n",
        "        inputs.canvas_color = Firefly::Canvas::base(document, CSS::SystemColor::canvas(color_scheme));\n",
    ),
}


def adapt(source, seam, hook):
    if source.count(seam) != 1 or PREFIX in source or hook in source:
        raise ValueError("Review native canvas paint seam")
    return PREFIX + source.replace(seam, hook)


def prepare(output_directory, root=ROOT):
    outputs = {}
    for name, (relative, digest, seam, hook) in ENTRIES.items():
        path = root / "vendor/ladybird" / relative
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise ValueError("Review pinned native canvas seam: " + relative)
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
    output_directory.mkdir(parents=True, exist_ok=True)
    for name, output in outputs.items():
        path = output_directory / name
        if not path.exists() or path.read_text() != output:
            path.write_text(output)
    print("PASS native canvas fingerprints, byte preservation and missing/duplicate seam controls")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.output)


if __name__ == "__main__":
    main()
