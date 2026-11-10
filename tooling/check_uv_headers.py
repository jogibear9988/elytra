#!/usr/bin/env python3
"""Keep owned libuv declarations behind the reviewed upstream/WinSDK type boundary."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
BOUNDARY = Path("mite/runtime/platform/UV.h")
DIRECT = re.compile(r'^\s*#\s*include\s*[<"]uv(?:\.h|/)', re.M)


def check(root=ROOT):
    rejected = []
    for directory in (root / "mite", root / "firefly"):
        for source in directory.rglob("*"):
            if source.suffix not in (".cpp", ".h", ".mm"):
                continue
            relative = source.relative_to(root)
            if relative != BOUNDARY and DIRECT.search(source.read_text()):
                rejected.append(str(relative))
    if rejected:
        raise ValueError("Use platform/UV.h for owned libuv headers: " + ", ".join(rejected))
    print("PASS owned libuv headers use the reviewed Windows type boundary")


if __name__ == "__main__":
    check()
