#!/usr/bin/env python3
"""Compare actual borrowed-fd Writable output and EOF with Node through OS pipes."""

import _support  # Standalone regression imports.

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("A real Node reference is required")
    expected = {
        "raw": (bytes(range(256)) * 256 + "Grüße 🦋\n".encode(), "native stderr 🦋\n".encode()),
        "partial": ("partial Grüße 🦋\nafter end/destroy\n".encode(), b"stderr descriptor alive\n"),
    }
    for name, (out, err) in expected.items():
        source = ROOT / "mite/tests/process-stdio" / f"{name}.mjs"
        for binary in [args.node, args.mite]:
            result = subprocess.run([binary, str(source)], capture_output=True, timeout=15)
            assert result.returncode == 0, (binary, name, result)
            assert result.stdout == out and result.stderr == err, (binary, name, result)
        print(f"PASS process stdio {name}: actual Node/Mite pipe bytes, callbacks and EOF")


if __name__ == "__main__":
    main()
