#!/usr/bin/env python3
"""Focused genuine Node/Mite byte primitive differential, without comparing entropy."""

import _support
import argparse
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    fixture = ROOT / "mite/tests/crypto-byte-primitives/main.mjs"

    def run(binary):
        result = subprocess.run(
            [str(binary), str(fixture)], check=True, capture_output=True, text=True, timeout=15
        )
        return json.loads(result.stdout)

    reference = run(args.node)
    if args.mite:
        assert run(args.mite) == reference
    print(
        "PASS byte primitive reference"
        + (" and actual Mite differential" if args.mite else " (Mite not run)")
    )


if __name__ == "__main__":
    main()
