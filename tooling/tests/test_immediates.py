#!/usr/bin/env python3
"""Compare actual check-phase callbacks, cancellation and liveness with pinned Node."""

import _support  # Standalone test import paths.

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
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for differential acceptance")
    fixtures = ROOT / "mite/tests/timers"
    count = 0
    with tempfile.TemporaryDirectory(prefix="mite-immediates-") as directory:
        root = Path(directory)
        for source in sorted(fixtures.iterdir()):
            if source.suffix not in {".mjs", ".cjs"}:
                continue
            suffixes = [source.suffix]
            if source.stem in {
                "fifo",
                "handles",
                "cancel",
                "unref-only",
                "unref-live",
                "failure",
                "validation",
            }:
                suffixes.append(".cjs")
            for suffix in suffixes:
                file = root / (source.stem + suffix)
                file.write_text(source.read_text())
                results = [
                    subprocess.run([binary, str(file)], text=True, capture_output=True, timeout=15)
                    for binary in [args.node, args.mite]
                ]
                status = 1 if source.stem == "failure" else 0
                assert all(result.returncode == status for result in results), (file.name, results)
                assert results[0].stdout == results[1].stdout, (file.name, results)
                if status:
                    assert all(
                        "immediate failure fixture" in result.stderr for result in results
                    ), results
                else:
                    assert all(not result.stderr for result in results), results
                assert "UNEXPECTED" not in results[1].stdout, results
                print("PASS immediate:", file.name, flush=True)
                count += 1
    print(f"PASS {count} actual Node/Mite immediate comparisons")


if __name__ == "__main__":
    main()
