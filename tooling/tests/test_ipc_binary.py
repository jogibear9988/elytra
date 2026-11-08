#!/usr/bin/env python3
"""Run real raw binary IPC through the production helper, optionally comparing Electron."""

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
    with tempfile.TemporaryDirectory(prefix="firefly-ipc-binary-") as directory:
        root = Path(directory)
        shutil.copytree(ROOT / "firefly/tests/ipc/binary", root, dirs_exist_ok=True)
        if args.electron:
            for file in root.iterdir():
                if file.suffix in [".mjs", ".html"]:
                    file.write_text(
                        file.read_text()
                        .replace("from 'firefly'", "from 'electron'")
                        .replace("const limited = true;", "const limited = false;")
                    )
        subprocess.run(
            [
                "node",
                str(ROOT / "tooling/bundle_preload.mjs"),
                str(root / "preload.mjs"),
                str(root / "preload.cjs"),
            ],
            check=True,
        )
        binary = (args.electron or args.firefly).resolve()
        result = run(binary, root / "main.mjs", *(["--reference"] if args.electron else []))
        assert result.returncode == 0 and "PASS public raw binary IPC:" in result.stdout, result
        assert (
            "VERIFICATION FAILED" not in result.stderr and "ERR_FIREFLY_IPC_" not in result.stderr
        ), result
        print(("Electron reference: " if args.electron else "") + result.stdout.strip())


if __name__ == "__main__":
    main()
