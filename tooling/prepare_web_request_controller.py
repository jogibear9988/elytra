#!/usr/bin/env python3
"""Compose private cancellation into the pinned Fetch controller without changing upstream."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "aa0f5b44370047ae64e93eade02a2508ba515a47c17da609e69380510b5ea0b3"
PREFIX = '#include "loading/web-request/Gate.h"\n'
SEAM = "void FetchController::cancel_webdriver_bidi_network_interception() const\n{\n"
HOOK = "    if (auto params = original_fetch_params())\n        Firefly::WebRequestGate::cancel(*params);\n"


def adapt(source):
    if source.count(SEAM) != 1 or PREFIX in source or HOOK in source:
        raise ValueError("Review private Fetch cancellation seam")
    return PREFIX + source.replace(SEAM, SEAM + HOOK)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    path = ROOT / "vendor/ladybird/Libraries/LibWeb/Fetch/Infrastructure/FetchController.cpp"
    if hashlib.sha256(path.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit("Review pinned Fetch controller before updating Ladybird")
    source = path.read_text()
    output = adapt(source)
    assert output.removeprefix(PREFIX).replace(SEAM + HOOK, SEAM) == source
    for invalid in ("", source.replace(SEAM, ""), source + SEAM, output):
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Missing/duplicate cancellation seam accepted")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != output:
        args.output.write_text(output)
    print("PASS private Fetch cancellation fingerprint, byte preservation and drift controls")


if __name__ == "__main__":
    main()
