#!/usr/bin/env python3
"""Compare controlled networking policy in Node/Mite without opening endpoints."""

import _support

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
        parser.error("A real Node runtime is required")
    observations = []
    for binary in [args.node, str(args.mite.resolve())]:
        result = subprocess.run(
            [binary, str(ROOT / "firefly/tests/net/policy.mjs")],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=15,
        )
        assert result.returncode == 0 and not result.stderr, result
        observations.append(result.stdout)
    assert observations[0] == observations[1], observations
    lines = observations[0].splitlines()
    assert len(lines) >= 7 and lines[-1] == "DONE controlled net policy", lines
    assert all(line.startswith("PASS ") for line in lines[:-1]), lines
    print(observations[1], end="")
    print("PASS Node/Mite controlled net policy; no endpoint requests or OS network changes")


if __name__ == "__main__":
    main()
