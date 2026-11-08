#!/usr/bin/env python3
"""Validate genuine native filesystem-event admission, embedded GC and shutdown ownership."""

import _support

import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--host", required=True, type=Path)
    args = parser.parse_args()
    cases = [
        (
            args.mite,
            "capacity",
            "PASS actual event watcher 256 active/closing cap, native-drain recovery and VM shutdown\n",
            "",
        ),
        (
            args.host,
            "gc-lifetime",
            "PASS actual native event watcher active roots, close/GC callback reentry and final reclamation\n",
            "HOST_SHUTDOWN\n",
        ),
    ]
    for binary, fixture, output, error in cases:
        with tempfile.TemporaryDirectory(prefix="mite-event-lifetime-") as directory:
            result = subprocess.run(
                [
                    str(binary.resolve()),
                    str(ROOT / "mite/tests/event-watcher" / (fixture + ".mjs")),
                ],
                cwd=directory,
                capture_output=True,
                text=True,
                timeout=12,
            )
        assert result.returncode == 0 and result.stdout == output and result.stderr == error, (
            fixture,
            result,
        )
        print(result.stdout, end="", flush=True)


if __name__ == "__main__":
    main()
