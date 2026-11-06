#!/usr/bin/env python3
"""Compose a pinned file-script condition without editing Ladybird's fetch implementation."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "1dd94814cadf7d93872b5b81f67bcb9b1b16d24ce0fd681328652fbb09dabe85"
SEAM = "            || is_file_to_file_font_load\n"
HOOK = "            || Firefly::FileModules::allows(*request)\n"
PREFIX = '#include "loading/FileModules.h"\n'


def adapt(source):
    if source.count(SEAM) != 1:
        raise ValueError("Review the file module fetch condition")
    return PREFIX + source.replace(SEAM, SEAM + HOOK)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = ROOT / "vendor/ladybird/Libraries/LibWeb/Fetch/Fetching/Fetching.cpp"
    if hashlib.sha256(source.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit("Review owned file module adapter before updating Ladybird")
    original = source.read_text()
    output = adapt(original)
    assert output.removeprefix(PREFIX).replace(SEAM + HOOK, SEAM) == original
    for invalid in ["", original + SEAM, original.replace(SEAM, "")]:
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Missing/ambiguous fetch seam accepted")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != output:
        args.output.write_text(output)
    print(
        "PASS file module fetch seam, source fingerprint, exact byte preservation and drift rejection"
    )


if __name__ == "__main__":
    main()
