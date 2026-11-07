#!/usr/bin/env python3
"""Run real upstream navigation population over native resource IPC without desktop windows."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly navigation ä ") as temporary:
        path = Path(temporary) / "index 世界.html"
        prefix = b"<!doctype html><title>NAVIGATION_OK</title><p>native-navigation</p>\n"
        path.write_bytes(prefix + b"x" * (256 * 1024 + 19 - len(prefix)))
        oversized = Path(temporary) / "oversized 世界.html"
        with oversized.open("wb") as output:
            output.truncate(16 * 1024 * 1024 + 1)
        boundary = Path(temporary) / "boundary 世界.html"
        boundary.write_bytes(prefix + b"x" * (16 * 1024 * 1024 - len(prefix)))
        for mode in [
            "body",
            "boundary",
            "scope",
            "default",
            "page",
            "url",
            "replay",
            "revoke",
            "replace",
            "peerloss",
            "oversize",
        ]:
            result = subprocess.run(
                [
                    str(args.probe.resolve()),
                    mode,
                    str(
                        oversized
                        if mode == "oversize"
                        else boundary
                        if mode == "boundary"
                        else path
                    ),
                ],
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=45,
            )
            assert result.returncode == 0, result
            assert (
                f"PASS actual navigation population over native resource channel {mode}:"
                in result.stdout
            ), result
            assert (
                f"PASS navigation main-side file admission and reaped child {mode}" in result.stdout
            ), result
            assert "VERIFICATION FAILED" not in result.stderr, result
            print(result.stdout.strip(), flush=True)
    print(
        "PASS native navigation population; live helper/bootstrap and document commit remain separate gates"
    )


if __name__ == "__main__":
    main()
