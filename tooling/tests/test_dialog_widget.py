#!/usr/bin/env python3
"""Bound the controlled Qt widget probe; a stalled native helper must not hang CI."""

import _support
import argparse
import os
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True)
    args = parser.parse_args()
    result = subprocess.run(
        [args.probe],
        env=dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QPA_PLATFORMTHEME="generic"),
        capture_output=True,
        text=True,
        timeout=10,
    )
    print(result.stdout, end="")
    if result.returncode:
        raise SystemExit(result.stderr or result.returncode)
    assert "PASS exact production QMessageBox offscreen" in result.stdout, result


if __name__ == "__main__":
    main()
