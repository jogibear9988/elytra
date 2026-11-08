#!/usr/bin/env python3
"""Exercise genuine embedded LibJS collection in CJS original-export and namespace snapshots."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, type=Path)
    options = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mite-cjs-namespace-gc-") as directory:
        source = Path(directory) / "sources"
        shutil.copytree(ROOT / "mite/tests/cjs-namespaces/gc", source)
        result = subprocess.run(
            [str(options.host.resolve()), str(source / "main.mjs")],
            cwd=directory,
            capture_output=True,
            text=True,
            timeout=20,
        )
    expected = (
        "PASS native CJS original exports survive pre-import collection\n"
        "PASS native CJS snapshot roots survive getter-triggered collection\n"
        "PASS private CJS parser and namespace cache survive collection\n"
    )
    assert result.returncode == 0 and result.stdout == expected, result
    assert result.stderr == "HOST_SHUTDOWN\n", result
    print(
        "PASS actual native CJS original-export cache, getter snapshot and private parser GC ownership"
    )


if __name__ == "__main__":
    main()
