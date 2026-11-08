#!/usr/bin/env python3
"""Run actual embedded LibJS GC while genuine FileHandle WriteStream I/O is pending."""

import _support

import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, type=Path)
    options = parser.parse_args()
    fixture = ROOT / "mite/tests/file-write-stream/gc-lifetime.mjs"
    with tempfile.TemporaryDirectory(prefix="mite-write-stream-gc-") as directory:
        result = subprocess.run(
            [str(options.host.resolve()), str(fixture)],
            cwd=directory,
            capture_output=True,
            text=True,
            timeout=40,
        )
        expected = (
            "PASS real native FileHandle WriteStream GC lifetime end\n"
            "PASS real native FileHandle WriteStream GC lifetime manual-close\n"
        )
        assert result.returncode == 0 and result.stdout == expected, result
        assert result.stderr == "HOST_SHUTDOWN\n", result
        for name, fill in [("gc-end.bin", 0x2A), ("gc-manual-close.bin", 0x73)]:
            actual = (Path(directory) / name).read_bytes()
            assert actual == bytes([fill]) * (8 * 1024 * 1024) + bytes([0, 255, fill]), name
    print(
        "PASS pending native WriteStream collection, queued close, descriptor retirement and independent file bytes"
    )


if __name__ == "__main__":
    main()
