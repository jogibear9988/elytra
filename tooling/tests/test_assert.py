#!/usr/bin/env python3
"""Compare real Node/Mite assertion outcomes, metadata, exceptions and GC behavior."""

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def run(command):
    result = subprocess.run(
        [str(x) for x in command], cwd=ROOT, capture_output=True, text=True, timeout=30
    )
    assert result.returncode == 0, (command, result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--host", required=True, type=Path)
    args = parser.parse_args()
    node = shutil.which("node")
    assert node
    expected = run([node, "--expose-gc", "mite/tests/assert/core.mjs"])
    for binary in [args.mite.resolve(), args.host.resolve()]:
        actual = run([binary, "mite/tests/assert/core.mjs"])
        assert actual.stdout == expected.stdout, (actual, expected)
        assert actual.stderr == ("HOST_SHUTDOWN\n" if binary == args.host.resolve() else ""), actual
    policy = run([args.mite.resolve(), "mite/tests/assert/policy.mjs"])
    assert "PASS explicit assert" in policy.stdout, policy
    print(
        "PASS Node/Mite assert identities, equality/deep collections, AssertionError metadata, sync/async exception contracts, forced GC and explicit unsupported cases"
    )


if __name__ == "__main__":
    main()
