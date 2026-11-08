#!/usr/bin/env python3
"""Compare actual Node24/Mite legacy Url parsing and validate immutable upstream source pins."""

import _support

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def run(binary):
    result = subprocess.run(
        [binary, str(ROOT / "mite/tests/url-legacy/parse.mjs")],
        cwd=ROOT,
        env={**os.environ, "NODE_NO_WARNINGS": "1"},
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert result.returncode == 0, (binary, result.stdout, result.stderr)
    assert not result.stderr, (binary, result.stderr)
    return json.loads(result.stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    manifest = json.loads((ROOT / "vendor/node-url/manifest.json").read_text())
    for source in manifest["sources"]:
        data = (ROOT / "vendor/node-url" / source["path"]).read_bytes()
        assert hashlib.sha256(data).hexdigest() == source["sha256"], source["path"]
        if source["path"] == "url.js":
            assert b"Copyright Joyent" in data and b"Permission is hereby granted" in data
    assert args.node, "Node24 reference is required"
    reference = run(args.node)
    actual = run(args.mite)
    assert len(actual) == len(reference)
    for index, (expected, observed) in enumerate(zip(reference, actual)):
        assert observed == expected, (index, expected, observed)
    print(
        f"PASS {len(actual)} genuine Node/Mite legacy URL parsing/query/format cases and exact Node24 source pins"
    )


if __name__ == "__main__":
    main()
