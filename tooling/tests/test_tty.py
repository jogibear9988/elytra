#!/usr/bin/env python3
"""Compare read-only descriptor classification on real pipes, files and headless PTYs."""

import argparse
import errno
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "mite/tests/tty/main.mjs"


def observation(binary, file, terminal=False):
    master = slave = None
    try:
        if terminal:
            import pty

            master, slave = pty.openpty()
        result = subprocess.run(
            [str(binary), str(FIXTURE), str(file)],
            stdin=slave if terminal else subprocess.DEVNULL,
            stdout=slave if terminal else subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=10,
        )
        assert result.returncode == 0 and not result.stderr, result
        if terminal:
            # Darwin can discard unread PTY output when its final slave closes.
            # The completed child has flushed all writes; drain before our close.
            os.set_blocking(master, False)
            output = bytearray()
            while True:
                try:
                    chunk = os.read(master, 4096)
                except OSError as error:
                    if error.errno not in (errno.EIO, errno.EAGAIN, errno.EWOULDBLOCK):
                        raise
                    break
                if not chunk:
                    break
                output.extend(chunk)
                assert len(output) <= 4096, "Unexpected terminal output"
        else:
            output = result.stdout
        return json.loads(output)
    finally:
        if slave is not None:
            os.close(slave)
        if master is not None:
            os.close(master)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.node or (not args.reference_only and not args.mite):
        parser.error("Require Node and --mite, or --reference-only")
    runtimes = [("Node", args.node)]
    if not args.reference_only:
        runtimes.append(("Mite", args.mite.resolve()))
    with tempfile.TemporaryDirectory(prefix="mite-tty-") as directory:
        file = Path(directory) / "owned-file"
        file.write_bytes(b"regular file, not a terminal")
        for name, binary in runtimes:
            assert observation(binary, file) == {
                "descriptors": [False, False, False],
                "checked": True,
            }
            print(f"PASS {name} pipe/file/closed/invalid descriptors without coercion", flush=True)
            if os.name != "nt":
                assert observation(binary, file, terminal=True) == {
                    "descriptors": [True, True, False],
                    "checked": True,
                }
                print(f"PASS {name} real headless PTY positive detection", flush=True)
            else:
                print("SKIP headless PTY: Python POSIX PTY fixture unavailable on Windows")


if __name__ == "__main__":
    main()
