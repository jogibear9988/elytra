#!/usr/bin/env python3
"""Compare title-bar admission with real Node and Mite runtimes."""

import _support
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    assert args.node, "A real Node reference is required"
    results = [
        subprocess.run(
            [str(binary), str(ROOT / "firefly/tests/title-bar/policy.mjs")],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=20,
        )
        for binary in [args.node, args.mite.resolve()]
    ]
    assert all(result.returncode == 0 and not result.stderr for result in results), results
    assert results[0].stdout == results[1].stdout, results
    assert len(results[1].stdout.splitlines()) == 4, results
    print(results[1].stdout, end="")
    print("PASS controlled Node/Mite title-bar policy; native chrome needs its own gate")


if __name__ == "__main__":
    main()
