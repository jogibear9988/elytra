#!/usr/bin/env python3
"""Exercise real native two-process demand-driven response producers without a GUI/VM."""

import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    args = parser.parse_args()
    binary = args.probe.resolve()
    for case in [
        "known",
        "unknown",
        "duplicate",
        "head",
        "bodyless",
        "cancel",
        "stale",
        "authorization-stale",
        "revoke",
        "error",
        "oversize",
        "short",
        "pull-bound",
        "quota",
    ]:
        result = subprocess.run([str(binary), case], capture_output=True, text=True, timeout=25)
        expected = f"PASS real two-process stream server {case}: demand, checked frames and retired producer ownership\n"
        assert result.returncode == 0 and result.stdout == expected and not result.stderr, (
            case,
            result,
        )
        print(result.stdout, end="", flush=True)
    print(
        "PASS real native response producers; no public protocol.handle/renderer integration is inferred"
    )


if __name__ == "__main__":
    main()
