#!/usr/bin/env python3
"""Prepare the pinned native dependency manager; no global packages are installed."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
VCPKG_REVISION = "20ceb8adbf0504d4b0d5e4058a32f9d4fc896391"
LADYBIRD_REVISION = "51ef0d743ceca25863847f8cea208c1c6f7031ce"


def run(*args, cwd=ROOT):
    subprocess.run(list(map(str, args)), cwd=cwd, check=True)


def main():
    run("git", "submodule", "update", "--init", "--recursive")
    revision = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT / "vendor/ladybird", text=True).strip()
    if revision != LADYBIRD_REVISION:
        raise SystemExit("Ladybird revision differs from the reviewed build adapter")
    vcpkg = ROOT / "build/vcpkg"
    vcpkg.parent.mkdir(exist_ok=True)
    if not vcpkg.exists():
        run("git", "clone", "--no-checkout",
            "https://github.com/microsoft/vcpkg.git", vcpkg)
    shallow = subprocess.check_output(
        ["git", "rev-parse", "--is-shallow-repository"], cwd=vcpkg, text=True).strip()
    if shallow == "true":
        run("git", "fetch", "--unshallow", "origin", cwd=vcpkg)
    run("git", "fetch", "origin", VCPKG_REVISION, cwd=vcpkg)
    run("git", "checkout", "--detach", VCPKG_REVISION, cwd=vcpkg)
    if os.name == "nt":
        run("cmd", "/c", vcpkg / "bootstrap-vcpkg.bat", "-disableMetrics")
    else:
        run(vcpkg / "bootstrap-vcpkg.sh", "-disableMetrics")
    print("Dependencies pinned. Configure using the README build commands.")


if __name__ == "__main__":
    main()
