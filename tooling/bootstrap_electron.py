#!/usr/bin/env python3
"""Install the pinned behavioral reference in ignored build/, never in Firefly."""

from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    source = ROOT / "tooling/electron-reference"
    target = ROOT / "build/electron-reference"
    target.mkdir(parents=True, exist_ok=True)
    for name in ["package.json", "package-lock.json"]:
        shutil.copyfile(source / name, target / name)
    subprocess.run(["npm", "ci", "--prefix", str(target), "--no-audit", "--no-fund"], check=True)
    # The pinned package exposes an explicit installer for its native binary.
    subprocess.run(["node", str(target / "node_modules/electron/install.js")], check=True)
    print(
        target
        / "node_modules/electron/dist"
        / ((target / "node_modules/electron/path.txt").read_text().strip())
    )


if __name__ == "__main__":
    main()
