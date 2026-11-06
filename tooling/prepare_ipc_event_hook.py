#!/usr/bin/env python3
"""Prepare the owned native IPC event hook without changing upstream sources."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "cd63de48c088f7d0127206ea464cd073728a6c645a117539c32326c49cf97930"
SEAM = "    if (auto* webdriver_bidi = WebDriverBiDi::the())\n        webdriver_bidi->did_receive_event(*this, navigable_id, name, move(params));"
HOOK = "    if (Firefly::receive_native_ipc(*this, navigable_id, name, params)) return;\n"


def adapt(source):
    if source.count(SEAM) != 1:
        raise ValueError("Review WebContentPage IPC seam")
    return '#include "TransportHooks.h"\n' + source.replace(SEAM, HOOK + SEAM)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    path = ROOT / "vendor/ladybird/Libraries/LibWebView/WebContentPage.cpp"
    if hashlib.sha256(path.read_bytes()).hexdigest() != EXPECTED:
        raise SystemExit("Review owned native IPC event adapter before updating Ladybird")
    original = path.read_text()
    output = adapt(original)
    assert (
        output.removeprefix('#include "TransportHooks.h"\n').replace(HOOK + SEAM, SEAM) == original
    )
    for invalid in ["", original + SEAM]:
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Ambiguous IPC seam accepted")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != output:
        args.output.write_text(output)
    print("PASS owned native IPC event seam, upstream fingerprint and byte preservation")


if __name__ == "__main__":
    main()
