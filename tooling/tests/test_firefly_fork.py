#!/usr/bin/env python3
"""Accept real same-executable fork with no windows and an invalid child Qt plugin."""

import _support

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", required=True, type=Path)
    parser.add_argument("--offscreen", action="store_true")
    args = parser.parse_args()
    environment = {**os.environ, "LIBGL_ALWAYS_SOFTWARE": "1"}
    if args.offscreen:
        environment["QT_QPA_PLATFORM"] = "offscreen"
    with tempfile.TemporaryDirectory(prefix="elytra-fork-host-") as directory:
        result = subprocess.run(
            [
                str(args.firefly.resolve()),
                "--electron-compat",
                str(ROOT / "mite/tests/fork/firefly-main.mjs"),
                "--user-data-dir=" + directory,
                "--force-cpu-painting",
            ],
            cwd=ROOT,
            env=environment,
            capture_output=True,
            text=True,
            timeout=20,
        )
        assert result.returncode == 0, result
        assert "PASS production Firefly parent forks plain Mite child" in result.stdout, result
        print(result.stdout, end="")
        if result.stderr:
            print(result.stderr, end="")
        print(
            "PASS same-executable fork boundary; no window created, no utility parentPort or desktop imports in child"
        )


if __name__ == "__main__":
    main()
