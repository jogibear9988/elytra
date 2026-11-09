#!/usr/bin/env python3
"""Compare the real spellcheck preference policy under Node and Mite; no renderer or GUI."""

import _support
import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    assert args.node, "A real Node reference is required"
    expected = None
    for name, binary in [("Node", args.node), ("Mite", args.mite.resolve())]:
        result = subprocess.run(
            [str(binary), str(ROOT / "firefly/tests/spellcheck/policy.mjs")],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=15,
        )
        assert result.returncode == 0 and not result.stderr, (name, result)
        assert len(result.stdout.splitlines()) == 4 and all(
            line.startswith("PASS ") for line in result.stdout.splitlines()
        ), (name, result)
        if expected is None:
            expected = result.stdout
        assert result.stdout == expected, (name, expected, result)
    print(expected, end="", flush=True)
    print(
        "PASS Node/Mite controlled provider policy only; actual provider-null engine acceptance is separate"
    )


if __name__ == "__main__":
    main()
