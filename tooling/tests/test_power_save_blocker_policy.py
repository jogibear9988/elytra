#!/usr/bin/env python3
"""Run controlled powerSaveBlocker policy; never request a real OS assertion."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("A real Node runtime is required for the shared policy fixture")
    results = [
        subprocess.run(
            [str(binary), str(ROOT / "firefly/tests/power-save-blocker/policy.mjs")],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=20,
        )
        for binary in [args.node, args.mite.resolve()]
    ]
    assert all(result.returncode == 0 and not result.stderr for result in results), results
    assert results[0].stdout == results[1].stdout, results
    lines = results[0].stdout.splitlines()
    assert len(lines) == 6 and lines[-1] == "DONE power save blocker policy", results
    assert all(line.startswith("PASS ") for line in lines[:-1]), results
    print(results[1].stdout, end="")
    print("PASS Node/Mite controlled policy only; no real OS sleep-prevention assertions")


if __name__ == "__main__":
    main()
