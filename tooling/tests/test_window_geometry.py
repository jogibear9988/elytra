#!/usr/bin/env python3
"""Compare BrowserWindow geometry policy without GUI; optionally run checked native frame math."""

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
    parser.add_argument("--probe", type=Path)
    args = parser.parse_args()
    if not args.node:
        parser.error("Install the pinned Node reference first")
    if args.probe:
        probe = subprocess.run(
            [str(args.probe.resolve())], capture_output=True, text=True, timeout=10
        )
        assert probe.returncode == 0 and not probe.stderr, probe
        assert probe.stdout.startswith("PASS native checked outer/content DIP translation"), probe
        print(probe.stdout, end="")
    results = [
        subprocess.run(
            [binary, str(ROOT / "firefly/tests/window/geometry-policy.mjs")],
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
    assert len(lines) == 4 and lines[-1] == "DONE geometry policy", results
    assert all(line.startswith("PASS ") for line in lines[:-1]), results
    print(results[1].stdout, end="")
    print("PASS actual Node/Mite window geometry factory without GUI")


if __name__ == "__main__":
    main()
