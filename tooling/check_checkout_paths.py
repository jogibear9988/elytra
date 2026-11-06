#!/usr/bin/env python3
"""Reject tracked path components that ordinary native Windows checkouts cannot create."""

import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEVICE = re.compile(r"^(?:CON|PRN|AUX|NUL|COM[1-9¹²³]|LPT[1-9¹²³])(?:\.|$)", re.IGNORECASE)
FORBIDDEN = set('<>:"\\|?*')


def reason(path):
    for component in path.split("/"):
        if not component or component in (".", ".."):
            return "non-file path component"
        if DEVICE.match(component):
            return "reserved Windows device name"
        if any(char in FORBIDDEN or ord(char) < 32 for char in component):
            return "reserved Windows character"
        if component.endswith((" ", ".")):
            return "trailing space or period"
    return None


def tracked_paths(root):
    data = subprocess.check_output(["git", "-C", str(root), "ls-files", "--cached", "-z"])
    return [path.decode("utf-8") for path in data.split(b"\0") if path]


def check(paths):
    return [(path, problem) for path in paths if (problem := reason(path))]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args()
    paths = tracked_paths(args.root)
    failures = check(paths)
    for path, problem in failures:
        print(f"Invalid checkout path: {path!r}: {problem}")
    if failures:
        raise SystemExit(1)
    print(
        f"PASS {len(paths)} tracked paths: no reserved Win32 component names/characters or trailing spaces/periods"
    )


if __name__ == "__main__":
    main()
