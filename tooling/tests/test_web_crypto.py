#!/usr/bin/env python3
"""Compare genuine runtime WebCrypto entropy boundaries with Node; outputs contain no random bytes."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/web-crypto"


def run(binary, fixture):
    result = subprocess.run(
        [str(binary), str(FIXTURES / fixture)], cwd=ROOT, capture_output=True, text=True, timeout=20
    )
    assert result.returncode == 0 and not result.stderr, result
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("A real Node runtime is required for the differential baseline")
    expected = run(args.node, "main.mjs")
    actual = run(args.mite.resolve(), "main.mjs")
    assert expected == actual and actual.endswith("DONE WebCrypto randomness\n"), (expected, actual)
    print(actual, end="")
    print(run(args.mite.resolve(), "unsupported.mjs"), end="")


if __name__ == "__main__":
    main()
