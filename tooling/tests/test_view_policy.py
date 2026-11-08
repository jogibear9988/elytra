#!/usr/bin/env python3
"""Check view input/ownership policy in Node/Mite; this does not exercise native widgets."""

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
            [binary, str(ROOT / "firefly/tests/web-contents-view/policy.mjs")],
            capture_output=True,
            text=True,
            timeout=15,
            cwd=ROOT,
        )
        for binary in [args.node, args.mite]
    ]
    assert all(result.returncode == 0 and not result.stderr for result in results), results
    assert results[0].stdout == results[1].stdout, results
    lines = results[0].stdout.splitlines()
    assert len(lines) == 8 and lines[-1] == "DONE view policy", results
    assert all(line.startswith("PASS ") for line in lines[:-1]), results
    print(results[1].stdout, end="")
    print("PASS Node/Mite policy projection only; native composition acceptance is separate")


if __name__ == "__main__":
    main()
