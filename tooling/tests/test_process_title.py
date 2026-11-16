#!/usr/bin/env python3
"""Compare native process.title and optionally the unchanged xterm consumer with Node."""

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def run(binary, name, count, extra=()):
    result = subprocess.run(
        [str(binary), str(ROOT / "mite/tests/process-title" / name), *map(str, extra)],
        capture_output=True,
        text=True,
        timeout=25,
        cwd=ROOT,
    )
    if (
        result.returncode
        or sum(line.startswith("PASS ") for line in result.stdout.splitlines()) != count
    ):
        raise RuntimeError(
            f"{binary} {name}: exit={result.returncode}\n{result.stdout}\n{result.stderr}"
        )
    print(f"{Path(binary).name}: {name}: {count} groups PASS")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--stage", type=Path)
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    node = shutil.which("node")
    if not node:
        parser.error("Node reference required")
    if not args.reference_only and not args.mite:
        parser.error("--mite required")
    for binary in [node] + ([] if args.reference_only else [args.mite.resolve()]):
        run(binary, "main.mjs", 4)
        if args.stage:
            run(binary, "xterm.mjs", 1, [args.stage.resolve()])
    if not args.stage:
        print("SKIP optional pinned xterm: --stage not supplied")


if __name__ == "__main__":
    main()
