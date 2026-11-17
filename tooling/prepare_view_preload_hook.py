#!/usr/bin/env python3
"""Configure the canonical Hub view before author scripts, preserving upstream bytes."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "View.cpp": "97c5f4b0e6516b47870ed58abc8027bfa6b345e580f8a269c9e3871a0a3f8848",
    "View.h": "8dc42bcaec6624eb89f663f4bfe30361cc6ce351babf27d58b1c68c9a1c9a81b",
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
