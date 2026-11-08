#!/usr/bin/env python3
"""Require actual extension resolution and read-only native policy availability, without a GUI."""

import _support

import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    args = parser.parse_args()
    result = subprocess.run(
        [str(args.mite.resolve()), str(ROOT / "mite/tests/policy-watcher/public.mjs")],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=15,
    )
    assert result.returncode == 0 and not result.stderr, result
    assert result.stdout.startswith("PASS public policy extension singleton and "), result
    print(result.stdout, end="")


if __name__ == "__main__":
    main()
