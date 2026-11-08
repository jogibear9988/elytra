#!/usr/bin/env python3
"""Check real trusted getter publication, optionally against the Electron reference."""

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
    with tempfile.TemporaryDirectory(prefix="firefly-preload-getter-") as directory:
        root = Path(directory)
        shutil.copytree(ROOT / "firefly/tests/preload/getters", root, dirs_exist_ok=True)
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
        assert result.returncode == 0 and "PASS preload getter publication:" in result.stdout, (
            result
        )
        assert "VERIFICATION FAILED" not in result.stderr, result
        print(("Electron reference: " if args.electron else "") + result.stdout.strip())


if __name__ == "__main__":
    main()
