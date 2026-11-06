#!/usr/bin/env python3
"""Exercise the shared screen policy in Node/Mite; optionally query macOS without GUI."""

import _support  # Standalone regression imports.

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--probe", type=Path)
    args = parser.parse_args()
    if not args.node:
        parser.error("Install the pinned Node reference first")
    with tempfile.TemporaryDirectory(prefix="firefly-screen-") as directory:
        extra = []
        if args.probe:
            probe = subprocess.run(
                [str(args.probe.resolve())], capture_output=True, text=True, timeout=15
            )
            assert probe.returncode == 0, probe
            snapshot = Path(directory) / "native.json"
            snapshot.write_text(probe.stdout)
            extra.append(str(snapshot))
        results = [
            subprocess.run(
                [binary, str(ROOT / "firefly/tests/screen/policy.mjs"), *extra],
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
        assert len(lines) == (7 if args.probe else 6), results
        assert lines[-1] == "DONE screen policy", results
        assert all(line.startswith("PASS ") for line in lines[:-1]), results
        print(results[1].stdout, end="")
        print("PASS shared API factory in actual Node/Mite; no GUI application launched")


if __name__ == "__main__":
    main()
