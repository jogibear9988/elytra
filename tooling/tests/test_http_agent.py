#!/usr/bin/env python3
"""Compare real HTTP Agent admission and the optional pinned consumer in Node and Mite."""

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def run(binary, fixture, count, extra=()):
    result = subprocess.run(
        [str(binary), str(ROOT / "mite/tests/http-agent" / fixture), *map(str, extra)],
        capture_output=True,
        text=True,
        timeout=35,
        cwd=ROOT,
    )
    markers = [line for line in result.stdout.splitlines() if line.startswith("PASS ")]
    if result.returncode or len(markers) != count:
        raise RuntimeError(
            f"{binary} {fixture}: exit={result.returncode}\n{result.stdout}\n{result.stderr}"
        )
    print(f"{Path(binary).name}: {fixture}: {count} groups PASS")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--stage", type=Path)
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    node = shutil.which("node")
    if not node:
        parser.error("Node reference executable required")
    if not args.reference_only and not args.mite:
        parser.error("--mite is required unless --reference-only")
    binaries = [node] + ([] if args.reference_only else [args.mite.resolve()])
    for binary in binaries:
        run(binary, "main.mjs", 6)
        if args.stage:
            run(binary, "agent-base.mjs", 3, [args.stage.resolve()])
    if not args.stage:
        print("SKIP optional unchanged agent-base consumer: --stage not supplied")


if __name__ == "__main__":
    main()
