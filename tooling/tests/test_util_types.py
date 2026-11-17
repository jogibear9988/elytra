#!/usr/bin/env python3
"""Compare actual engine brands against Node without invoking author hooks."""

import _support  # noqa: F401 — shared standalone test import setup

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node reference is required")
    for fixture in ["util-types", "console"]:
        outputs = []
        for label, binary in [("Node", args.node), ("Mite", args.mite.resolve())]:
            result = subprocess.run(
                [str(binary), str(ROOT / "mite/tests" / fixture / "main.mjs")],
                cwd=ROOT,
                capture_output=True,
                text=True,
                timeout=15,
            )
            assert result.returncode == 0 and result.stdout.startswith("PASS "), (
                fixture,
                label,
                result.returncode,
                result.stdout,
                result.stderr,
            )
            outputs.append(result.stdout)
        assert outputs[0] == outputs[1], (fixture, outputs)
        print(outputs[1].strip())
    boundary = subprocess.run(
        [str(args.mite.resolve()), str(ROOT / "mite/tests/console/native-boundary.mjs")],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=15,
    )
    assert (
        boundary.returncode == 0
        and not boundary.stderr
        and boundary.stdout.strip()
        == "PASS native global Console stream hooks and UTF8/separator atomic bounds"
    ), boundary
    print(boundary.stdout.strip())


if __name__ == "__main__":
    main()
