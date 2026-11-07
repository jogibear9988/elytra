#!/usr/bin/env python3
"""Compare promisify and real native callback APIs with Node, including pending-callback GC."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/util"


def run(binary, source, *arguments, options=(), host=False):
    result = subprocess.run(
        [str(binary), *options, str(source), *map(str, arguments)],
        capture_output=True,
        text=True,
        timeout=25,
    )
    assert result.returncode == 0, result
    assert result.stderr == ("HOST_SHUTDOWN\n" if host else ""), result
    assert result.stdout and all(line.startswith("PASS ") for line in result.stdout.splitlines()), (
        result
    )
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    assert args.node
    source = FIXTURES / "promisify.mjs"
    reference = run(args.node, source)
    assert reference == run(args.mite.resolve(), source)
    print(reference, end="", flush=True)
    collected = run(args.node, source, options=["--expose-gc"])
    assert "PASS real GC with pending promisified callback and captured receiver\n" in collected
    assert collected == run(args.host.resolve(), source, host=True)
    print("PASS actual embedded-host GC with pending native-rooted callback", flush=True)
    with tempfile.TemporaryDirectory(prefix="mite promisify %# Grüße ") as directory:
        filename = Path(directory) / "data.txt"
        source = FIXTURES / "native.mjs"
        child = FIXTURES / "child.mjs"
        reference = run(args.node, source, filename, child)
        filename.unlink()
        assert reference == run(args.mite.resolve(), source, filename, child)
        print(reference, end="", flush=True)


if __name__ == "__main__":
    main()
