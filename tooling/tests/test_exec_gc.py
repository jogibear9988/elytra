#!/usr/bin/env python3
"""Check genuine shell capture/timeout across embedded LibJS GC, without GUI windows.

The POSIX timeout command replaces its shell with exec, ensuring the owned child
is killed directly. Windows shell/process replacement needs a separate fixture.
"""

import _support

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, type=Path)
    options = parser.parse_args()
    if os.name != "posix":
        print("SKIP exec GC: POSIX shell replacement fixture")
        return
    with tempfile.TemporaryDirectory(prefix="mite-exec-gc-") as directory:
        result = subprocess.run(
            [str(options.host.resolve()), str(ROOT / "mite/tests/exec/gc-lifetime.mjs")],
            cwd=directory,
            capture_output=True,
            text=True,
            timeout=20,
        )
    assert result.returncode == 0, result
    assert (
        result.stdout == "PASS real shell capture and timeout retain callbacks across native GC\n"
    ), result
    assert result.stderr == "HOST_SHUTDOWN\n", result
    print("PASS actual shell capture/timeout callback ownership across native LibJS collection")


if __name__ == "__main__":
    main()
