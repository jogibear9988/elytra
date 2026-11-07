#!/usr/bin/env python3
"""Check actual regular files, bounded native child streaming and explicit failures."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    probe = args.probe.resolve()
    with tempfile.TemporaryDirectory(prefix="firefly files ä ") as temporary:
        directory = Path(temporary)
        path = directory / "body 世界.bin"
        length = 4 * 1024 * 1024 + 19
        pattern = bytes((i * 37 + 11) & 255 for i in range(256))
        path.write_bytes((pattern * (length // 256 + 1))[:length])

        def run(operation, value):
            result = subprocess.run(
                [str(probe), operation, str(value)], capture_output=True, text=True, timeout=30
            )
            assert result.returncode == 0, result
            print(result.stdout.strip())
            return result.stdout

        streamed = run("transfer", path)
        assert f"bytes={length} chunks=65" in streamed, streamed
        assert f"HEAD length={length}" in run("head", path)
        run("cancel", path)
        run("refuse", directory)
        run("refuse", directory / "missing")
        run("refuse", "relative.bin")
        empty = directory / "empty"
        empty.touch()
        assert "bytes=0 chunks=0" in run("transfer", empty)
        if os.name != "nt":
            fifo = directory / "fifo"
            os.mkfifo(fifo)
            run("refuse", fifo)
            run("refuse", "/dev/null")
            huge = directory / "oversized"
            with huge.open("wb") as file:
                file.truncate(64 * 1024 * 1024 * 1024 + 1)
            run("refuse", huge)
        run("grow", path)
        run("replace", path)
        run("truncate", path)
    print(
        "PASS native file-body component; renderer requests, headers and public protocol remain separate integration work"
    )


if __name__ == "__main__":
    main()
