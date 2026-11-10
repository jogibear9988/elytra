#!/usr/bin/env python3
"""Configure the canonical Hub view before author scripts, preserving upstream bytes."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "View.cpp": "c5c85e95f83cf43ce5fcf0c7887baffec90e2ae9956c15db08d3d15856ca640c",
    "View.h": "1fa3b5dbb8ced278dd03b43818e916692992e5d7383a9861d1797d3eb52a4c3f",
}
PREFIX = '#include "native/qt/PreloadHooks.h"\n'

SEAM = "    ViewImplementation::prepare_page_for_navigable(page);"
HOOK = "\n    Firefly::prepare_preload_view(*this, page);"


def adapt(text):
    if text.count(SEAM) != 1 or PREFIX in text or HOOK in text:
        raise ValueError("Review canonical Hub view preparation seam")
    return PREFIX + text.replace(SEAM, SEAM + HOOK)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = ROOT / "vendor/ladybird/Services/Hub"
    for name, expected in EXPECTED.items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != expected:
            raise SystemExit("Review/remove canonical Hub preload preparation adapter: " + name)
    original = (source / "View.cpp").read_text()
    generated = adapt(original)
    assert generated.removeprefix(PREFIX).replace(SEAM + HOOK, SEAM) == original
    for changed in ["", original + "\n" + SEAM, original.replace(SEAM, ""), generated]:
        try:
            adapt(changed)
        except ValueError:
            pass
        else:
            raise AssertionError("Changed preparation seam accepted")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != generated:
        args.output.write_text(generated)
    print("PASS canonical Hub preload preparation source/ABI pin, exact bytes and seam rejection")


if __name__ == "__main__":
    main()
