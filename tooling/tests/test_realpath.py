#!/usr/bin/env python3
"""Compare native canonical-path results and ownership against actual Node."""

import _support

import argparse
import errno
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required")
    with tempfile.TemporaryDirectory(prefix="mite-realpath-") as directory:
        work = Path(directory)
        target = work / "target"
        target.mkdir()
        (target / "child.txt").write_text("canonical bytes")
        raw_bytes = False
        if os.name != "nt":
            try:
                os.mkdir(os.path.join(os.fsencode(work), b"raw\xff"))
                raw_bytes = True
            except OSError as error:
                if error.errno not in (errno.EILSEQ, errno.EINVAL):
                    raise
                print("SKIP non-UTF8 filename case: filesystem rejects such names")
        symlinks = True
        try:
            (work / "linked").symlink_to(target, target_is_directory=True)
            (work / "broken").symlink_to(work / "missing")
            (work / "loop").symlink_to(work / "loop")
        except OSError as error:
            if getattr(error, "winerror", None) != 1314:
                raise
            symlinks = False
            print("SKIP symlink-only cases: Windows account lacks symlink privilege")
        observations = []
        for name, binary in [("Node", args.node), ("Mite", str(args.mite.resolve()))]:
            result = subprocess.run(
                [
                    binary,
                    str(ROOT / "mite/tests/realpath/main.mjs"),
                    *(["--symlinks"] if symlinks else []),
                    *(["--raw-bytes"] if raw_bytes else []),
                ],
                cwd=work,
                capture_output=True,
                text=True,
                timeout=8,
            )
            assert result.returncode == 0 and not result.stderr, (name, result)
            observations.append(result.stdout)
        assert observations[0] == observations[1], observations
        print(observations[0], end="")
        print("PASS Node/Mite realpath; actual symlink cases=" + str(symlinks))


if __name__ == "__main__":
    main()
