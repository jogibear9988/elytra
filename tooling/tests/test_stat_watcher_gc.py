#!/usr/bin/env python3
"""Force real embedded LibJS collection around native uv_fs_poll callback teardown."""

import _support

import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mite-stat-gc-") as directory:
        result = subprocess.run(
            [str(args.host.resolve()), str(ROOT / "mite/tests/stat-watcher/gc-lifetime.mjs")],
            cwd=directory,
            capture_output=True,
            text=True,
            timeout=12,
        )
        assert result.returncode == 0, result
        assert (
            result.stdout
            == "PASS actual native stat watcher active roots, callback unwatch/GC reentry and final reclamation\n"
        ), result
        assert result.stderr == "HOST_SHUTDOWN\n", result
        assert (Path(directory) / "gc-poll.txt").read_text() == "native-changed"
    print("PASS genuine native stat watcher GC callback/handle ownership")


if __name__ == "__main__":
    main()
