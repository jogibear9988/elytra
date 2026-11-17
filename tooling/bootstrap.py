#!/usr/bin/env python3
"""Prepare the pinned native dependency manager; no global packages are installed."""

from pathlib import Path
import os
import subprocess
from check_upstream import check

ROOT = Path(__file__).resolve().parents[1]
VCPKG_REVISION = "8f363bdc11359ede4bc51e5eb32a2617538049c8"


def run(*args, cwd=ROOT):
    subprocess.run(list(map(str, args)), cwd=cwd, check=True)


def main():
    run("git", "submodule", "update", "--init", "--recursive")
    check(ROOT)
    vcpkg = ROOT / "build/vcpkg"
    vcpkg.parent.mkdir(exist_ok=True)
    if not vcpkg.exists():
        run("git", "clone", "--no-checkout", "https://github.com/microsoft/vcpkg.git", vcpkg)
    shallow = subprocess.check_output(
        ["git", "rev-parse", "--is-shallow-repository"], cwd=vcpkg, text=True
    ).strip()
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
