#!/usr/bin/env python3
"""Compare real Web Stream pressure, byte transfer and cancellation with Node."""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    fixture = ROOT / "mite/tests/web-streams/core.mjs"
    results = [
        subprocess.run([binary, str(fixture)], capture_output=True, text=True, timeout=20)
        for binary in [args.node, args.mite]
    ]
    assert all(result.returncode == 0 and not result.stderr for result in results), results
    assert results[0].stdout == results[1].stdout, results
    assert results[1].stdout.count("PASS ") == 15, results
    assert results[1].stdout.endswith("DONE web streams\n"), results
    print(results[1].stdout, end="")
    # Change a global before the first CommonJS lookup: the alias must use the
    # startup snapshot, rather than lazily copying application replacements.
    script = """
const original = ReadableStream;
globalThis.ReadableStream = null;
const web = require('node:stream/web');
if (web.ReadableStream !== original || require('stream/web') !== web)
  throw new Error('CommonJS constructor/cache identity');
console.log('PASS CommonJS startup snapshot');
"""
    with tempfile.TemporaryDirectory(prefix="mite-web-streams-") as directory:
        file = Path(directory) / "identity.cjs"
        file.write_text(script)
        for binary in [args.node, args.mite]:
            result = subprocess.run([binary, str(file)], capture_output=True, text=True, timeout=10)
            assert result.returncode == 0 and not result.stderr, result
            assert result.stdout == "PASS CommonJS startup snapshot\n", result
    print("PASS CommonJS startup snapshot")


if __name__ == "__main__":
    main()
