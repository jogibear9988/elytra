#!/usr/bin/env python3
"""Compare genuine util.inherits behavior and invalid-target side effects with Node."""

import _support  # Standalone regression imports.

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("A real Node reference is required")
    results = [
        subprocess.run(
            [binary, str(ROOT / "mite/tests/util/inherits.mjs")],
            capture_output=True,
            text=True,
            timeout=15,
        )
        for binary in [args.node, args.mite]
    ]
    assert all(result.returncode == 0 and not result.stderr for result in results), results
    assert results[0].stdout == results[1].stdout, results
    assert results[0].stdout.endswith("DONE inherits\n"), results
    print(results[1].stdout, end="")


if __name__ == "__main__":
    main()
