#!/usr/bin/env python3
"""Generate the owned Qt view preparation adapter, preserving all other bytes."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "WebContentView.cpp": "2061d5d514ec314af9b0c3ec06ee016ffb05f8a6a2101eacb400f7db7996b030",
    "WebContentView.h": "5684511a723f8806c448b9d5240a814e5a944a5e9e74d5426292198101758732",
}
SEAM = "    ViewImplementation::prepare_page_for_navigable(page);"
HOOK = "\n    Firefly::prepare_preload_view(*this, page);"


def adapt(text):
    if text.count(SEAM) != 1:
        raise ValueError("Review Qt view preparation seam")
    return '#include "PreloadHooks.h"\n' + text.replace(SEAM, SEAM + HOOK)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = ROOT / "vendor/ladybird/UI/Qt"
    for name, expected in EXPECTED.items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != expected:
            raise SystemExit("Review/remove owned Qt preload preparation adapter: " + name)
    original = (source / "WebContentView.cpp").read_text()
    generated = adapt(original)
    assert (
        generated.removeprefix('#include "PreloadHooks.h"\n').replace(SEAM + HOOK, SEAM) == original
    )
    for changed in ["", original + "\n" + SEAM]:
        try:
            adapt(changed)
        except ValueError:
            pass
        else:
            raise AssertionError("Changed preparation seam accepted")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != generated:
        args.output.write_text(generated)
    print(
        "PASS Qt preload preparation source/ABI pin, byte preservation and missing/duplicate rejection"
    )


if __name__ == "__main__":
    main()
