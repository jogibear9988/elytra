#!/usr/bin/env python3
"""Check clipboard policy in Node/Mite; optional native probe uses only a private pasteboard."""

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
        result = subprocess.run(
            [str(args.probe.resolve())], capture_output=True, text=True, timeout=20
        )
        assert result.returncode == 0 and not result.stderr, result
        lines = result.stdout.splitlines()
        assert len(lines) == 3 and all(line.startswith("PASS native ") for line in lines), result
        print(result.stdout, end="")
    results = [
        subprocess.run(
            [binary, str(ROOT / "firefly/tests/clipboard/policy.mjs")],
            capture_output=True,
            text=True,
            timeout=20,
            cwd=ROOT,
        )
        for binary in [args.node, args.mite]
    ]
    assert all(result.returncode == 0 and not result.stderr for result in results), results
    assert results[0].stdout == results[1].stdout, results
    lines = results[0].stdout.splitlines()
    assert len(lines) == 5 and lines[-1] == "DONE clipboard policy", results
    assert all(line.startswith("PASS ") for line in lines[:-1]), results
    print(results[1].stdout, end="")
    print("PASS actual Node/Mite factory; no general clipboard access or GUI launch")


if __name__ == "__main__":
    main()
