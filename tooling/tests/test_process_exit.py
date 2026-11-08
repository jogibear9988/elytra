#!/usr/bin/env python3
"""Compare genuine process exit event/code/continuation behavior with Node 24."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
EXPECTED = {
    "natural": (0, "BODY\nEXIT:0\n", False),
    "natural-code": (7, "BODY\nEXIT:3\n", False),
    "explicit": (6, "BODY\nEXIT:6\n", False),
    "explicit-code": (8, "EXIT:4\n", False),
    "explicit-undefined": (0, "EXIT:0\n", False),
    "nested": (9, "ONCE:5:remaining=1\n", False),
    "throw": (1, "ONCE:0\n", True),
    "natural-async": (0, "EXIT:0\n", False),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.node or (not args.reference_only and not args.mite):
        parser.error("Require Node and --mite, or --reference-only")
    executables = [("Node", args.node)]
    if not args.reference_only:
        executables.append(("Mite", str(args.mite.resolve())))
    for mode, expected in EXPECTED.items():
        for name, binary in executables:
            result = subprocess.run(
                [binary, str(ROOT / "mite/tests/process-exit/main.mjs"), mode],
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=10,
            )
            observed = (
                result.returncode,
                result.stdout,
                "controlled-exit-listener-failure" in result.stderr,
            )
            assert observed == expected, (name, mode, expected, result)
            if not expected[2]:
                assert not result.stderr, (name, mode, result)
            assert "FORBIDDEN" not in result.stdout, (name, mode, result)
        print(
            "PASS real process exit/"
            + mode
            + " ("
            + ", ".join(name for name, _ in executables)
            + ")",
            flush=True,
        )


if __name__ == "__main__":
    main()
