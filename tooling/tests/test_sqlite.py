#!/usr/bin/env python3
"""Run the actual Mite SQLite extension and independently inspect its closed disk database."""

import argparse
from pathlib import Path
import sqlite3
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mite-real-sqlite-") as directory:
        filename = Path(directory) / "state.vscdb"
        result = subprocess.run(
            [str(args.mite.resolve()), str(ROOT / "mite/tests/sqlite/main.mjs"), str(filename)],
            capture_output=True,
            text=True,
            timeout=30,
        )
        assert result.returncode == 0 and not result.stderr, result
        assert "PASS real SQLite 3.53.4 disk" in result.stdout, result
        assert filename.read_bytes().startswith(b"SQLite format 3\0")
        with sqlite3.connect(filename) as database:
            rows = dict(database.execute("SELECT key,value FROM ItemTable"))
            assert rows == {"text": "persisted-α\0tail", "bytes": bytes([0, 128, 255]), "empty": ""}
            assert database.execute("PRAGMA integrity_check").fetchone() == ("ok",)
        print(result.stdout.strip())
        print("PASS independent Python SQLite reads real persisted rows and integrity")


if __name__ == "__main__":
    main()
