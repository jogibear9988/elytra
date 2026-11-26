#!/usr/bin/env python3
"""Verify the production picker IPC with a hidden native Qt dialog fixture."""

import _support
import argparse
import os
from pathlib import Path
import tempfile

from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly-file-access-") as temporary:
        root = Path(temporary)
        profile = root / "profile"
        profile.mkdir()
        files = root / "files"
        files.mkdir()
        source = files / "selected.txt"
        source.write_text("original", encoding="utf-8")
        os.utime(source, (1_700_000_000, 1_700_000_000))
        target = files / "result.txt"
        os.environ.update(
            QT_QPA_PLATFORM="cocoa",
            ELYTRA_TEST_OPEN_FILE=str(source),
            ELYTRA_TEST_OPEN_DIRECTORY=str(files),
            ELYTRA_TEST_PROFILE=str(profile),
            ELYTRA_TEST_SAVE_FILE=str(target),
        )
        result = run(args.probe.resolve(), ROOT / "firefly/tests/file-picker/access.mjs")
        if result.returncode or "PASS File System Access WebIDL" not in result.stdout:
            raise SystemExit(f"{result.stdout}\n{result.stderr}")
        if target.read_bytes() != b"hello Elytra":
            raise SystemExit("Unexpected on-disk result")
        print(result.stdout.strip())


if __name__ == "__main__":
    main()
