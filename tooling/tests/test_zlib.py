#!/usr/bin/env python3
"""Compare real Node/Mite codecs, independent Python streams and native GC lifetime."""

import argparse
import gzip
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]


def run(command, expected):
    result = subprocess.run(
        [str(x) for x in command], cwd=ROOT, capture_output=True, text=True, timeout=60
    )
    assert result.returncode == 0 and expected in result.stdout, (command, result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True, type=Path)
    parser.add_argument("--host", required=True, type=Path)
    args = parser.parse_args()
    node = shutil.which("node")
    assert node
    for binary in [node, args.mite.resolve()]:
        run([binary, "mite/tests/compression/zlib.mjs"], "PASS zlib sync/async/stream")
        with tempfile.TemporaryDirectory(prefix="elytra-zlib-") as directory:
            root = Path(directory)
            data = os.urandom(2 * 1048576) + b"\0Unicode \xc3\xbc" * 1000
            (root / "payload").write_bytes(data)
            raw = zlib.compressobj(wbits=-15)
            for name, value in {
                "raw": raw.compress(data) + raw.flush(),
                "zlib": zlib.compress(data),
                "gzip": gzip.compress(data),
            }.items():
                (root / name).write_bytes(value)
            run(
                [binary, "mite/tests/compression/zlib-peer.mjs", directory],
                "PASS independent byte peers",
            )
            for name, bits in [("raw", -15), ("zlib", 15), ("gzip", 31)]:
                assert zlib.decompress((root / ("out-" + name)).read_bytes(), wbits=bits) == data
    run([args.mite.resolve(), "mite/tests/compression/zlib-policy.mjs"], "PASS explicit zlib")
    for command in [[node, "--expose-gc"], [args.host.resolve()]]:
        run(command + ["mite/tests/compression/zlib-lifetime.mjs"], "PASS zlib read pressure")
    print(
        "PASS Node/Mite/Python streaming zlib formats and actual native-host GC/pressure/cancellation"
    )


if __name__ == "__main__":
    main()
