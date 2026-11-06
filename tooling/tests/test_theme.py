#!/usr/bin/env python3
"""Compare theme policy in Node/Mite, optionally using the native read-only macOS probe."""

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
    with tempfile.TemporaryDirectory(prefix="firefly-theme-") as directory:
        extra = []
        if args.probe:
            native = subprocess.run(
                [str(args.probe.resolve())], capture_output=True, text=True, timeout=15
            )
            assert native.returncode == 0 and not native.stderr, native
            snapshot = Path(directory) / "native.json"
            snapshot.write_text(native.stdout)
            extra.append(str(snapshot))
        results = [
            subprocess.run(
                [binary, str(ROOT / "firefly/tests/theme/policy.mjs"), *extra],
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
        assert len(lines) == (6 if args.probe else 5), results
        assert lines[-1] == "DONE theme policy", results
        assert all(line.startswith("PASS ") for line in lines[:-1]), results
        print(results[1].stdout, end="")
        print("PASS actual Node/Mite theme factory; no global preference changes or GUI launch")


if __name__ == "__main__":
    main()
