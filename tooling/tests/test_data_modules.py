#!/usr/bin/env python3
"""Compare data-URL ESM loading with Node using real resolver hooks and modules."""

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/data-modules"


def run(binary, name, groups, *, gc=False):
    result = subprocess.run(
        [
            str(binary),
            *(["--expose-gc"] if gc else []),
            str(FIXTURES / name),
            *(["--require-gc"] if gc else []),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=25,
    )
    lines = result.stdout.splitlines()
    assert result.returncode == 0 and len(lines) == groups, result
    assert all(line.startswith("PASS ") for line in lines), result
    assert not result.stderr, result
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.node:
        parser.error("Node reference required")
    if not args.reference_only and not args.mite:
        parser.error("--mite required without --reference-only")
    reference = run(args.node, "main.mjs", 7)
    print(reference, end="", flush=True)
    collected = run(args.node, "main.mjs", 8, gc=True)
    print(collected.splitlines()[-1], flush=True)
    if args.reference_only:
        return
    assert run(args.mite.resolve(), "main.mjs", 7) == reference
    assert run(args.mite.resolve(), "main.mjs", 8, gc=True) == collected
    print(run(args.mite.resolve(), "mite-only.mjs", 3), end="", flush=True)
    print(run(args.mite.resolve(), "count-limit.mjs", 1), end="", flush=True)
    print("PASS Node/Mite data module differential")


if __name__ == "__main__":
    main()
