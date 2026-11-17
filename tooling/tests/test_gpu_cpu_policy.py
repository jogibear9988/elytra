#!/usr/bin/env python3
"""Run the owned GPU fixture's actual argv predicate without a renderer or GUI."""

import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--node", action="append", type=Path, default=[])
    parser.add_argument("--mite", type=Path, required=True)
    args = parser.parse_args()
    fixture = ROOT / "firefly/tests/gpu/cpu-policy-controls.mjs"
    for executable in [*(args.node or [Path("node")]), args.mite]:
        result = subprocess.run(
            [str(executable), str(fixture)], capture_output=True, text=True, timeout=15
        )
        assert result.returncode == 0, (
            str(executable),
            result.returncode,
            result.stdout,
            result.stderr,
        )
        assert result.stdout == "PASS compositor CPU argv 15 controls\n", result.stdout
        assert result.stderr == "", result.stderr
        print("PASS non-GUI compositor argv policy:", executable)


if __name__ == "__main__":
    main()
