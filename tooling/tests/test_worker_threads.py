#!/usr/bin/env python3
"""Compare real same-process LibJS worker isolation and native lifecycle with Node."""

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/worker-threads"
CASES = {
    "main.mjs": (
        25,
        [
            "PASS worker same PID, independent VM/global/environment, real clone snapshots and natural exit",
            "PASS CommonJS parentPort one-shot message precedes exit",
            "PASS terminate interrupts a real tight loop and joins before exit",
            "PASS process.exit remains local to worker",
            "PASS worker uncaught error and nonzero exit preserve parent lifetime",
        ],
    ),
    "terminate-unref.mjs": (
        10,
        ["PASS terminate re-references an unref worker until joined exit settles"],
    ),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.node or (not args.reference_only and not args.mite):
        parser.error("Require Node and --mite, or --reference-only")
    runtimes = [("Node", args.node)]
    if not args.reference_only:
        runtimes.append(("Mite", str(args.mite.resolve())))
    for fixture, (deadline, expected) in CASES.items():
        for name, binary in runtimes:
            result = subprocess.run(
                [binary, str(FIXTURES / fixture)],
                cwd=ROOT,
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=deadline,
            )
            assert result.returncode == 0 and not result.stderr, (name, fixture, result)
            assert result.stdout.splitlines() == expected, (name, fixture, result)
            print(f"PASS {name} {fixture}: {len(expected)} native worker checks", flush=True)
    print("PASS genuine worker threads: independent VM snapshots, interruption and joined teardown")


if __name__ == "__main__":
    main()
