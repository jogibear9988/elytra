#!/usr/bin/env python3
"""Run build-tool NLS integration and real Node/Mite localized-string acceptance."""

import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    args = parser.parse_args()
    node = shutil.which("node")
    if not node:
        raise RuntimeError("Install the documented Node build/reference tool")
    subprocess.run(
        [node, str(Path(__file__).with_suffix(".mjs")), str(args.mite.resolve())],
        check=True,
        timeout=60,
    )


if __name__ == "__main__":
    main()
