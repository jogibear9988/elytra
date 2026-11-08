#!/usr/bin/env python3
"""Compare genuine shell execution and buffered ChildProcess behavior with Node."""

import _support

import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]


@contextmanager
def execution_directory():
    directory = tempfile.mkdtemp(prefix="mite-shell-exec-")
    try:
        yield directory
    finally:
        # Killing cmd does not kill its bounded Python grandchild. Let that
        # timed child release its cwd after functional acceptance; never kill a tree.
        deadline = time.monotonic() + 5
        while True:
            try:
                shutil.rmtree(directory)
                break
            except PermissionError:
                if sys.platform != "win32" or time.monotonic() >= deadline:
                    raise
                time.sleep(0.05)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required for genuine exec differential tests")
    for name in ["main", "validation"]:
        observations = []
        with execution_directory() as directory:
            work = Path(directory) / "cwd with spaces ä文"
            work.mkdir()
            for executable in [args.node, str(args.mite.resolve())]:
                result = subprocess.run(
                    [executable, str(ROOT / "mite/tests/exec" / (name + ".mjs")), sys.executable],
                    cwd=work,
                    env={**os.environ, "PYTHONIOENCODING": "utf-8"},
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    timeout=20,
                )
                assert result.returncode == 0 and not result.stderr, result
                observations.append(json.loads(result.stdout))
                assert not list(work.iterdir()), (
                    "Shell validation or workflow created unexpected files"
                )
            assert observations[0] == observations[1], (name, observations)
            print("PASS real shell exec/" + name + " against Node", flush=True)
    if sys.platform == "win32":
        print(
            "Executed genuine Windows cmd branches; no other Windows platform support is inferred"
        )
    else:
        print(
            "PASS POSIX shell quoting/pipelines, env/cwd, native capture/termination, promisify child and no-exec validation"
        )


if __name__ == "__main__":
    main()
