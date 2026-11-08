#!/usr/bin/env python3
"""Check real native browser zoom, optionally against the Electron reference."""

import _support
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", required=True, type=Path)
    parser.add_argument("--electron", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly-preload-zoom-") as directory:
        root = Path(directory)
        shutil.copytree(ROOT / "firefly/tests/preload/zoom", root, dirs_exist_ok=True)
        if args.electron:
            for source in root.glob("*.mjs"):
                source.write_text(source.read_text().replace("from 'firefly'", "from 'electron'"))
        subprocess.run(
            [
                "node",
                str(ROOT / "tooling/bundle_preload.mjs"),
                str(root / "preload.mjs"),
                str(root / "preload.cjs"),
            ],
            check=True,
        )
        result = run(
            (args.electron or args.firefly).resolve(),
            root / "main.mjs",
            *(["--reference"] if args.electron else []),
        )
        assert result.returncode == 0 and "PASS webFrame zoom:" in result.stdout, result
        assert "VERIFICATION FAILED" not in result.stderr, result
        print(("Electron reference: " if args.electron else "") + result.stdout.strip())


if __name__ == "__main__":
    main()
