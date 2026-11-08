#!/usr/bin/env python3
"""Require actual JavaScript fetch over native parent/child resource IPC, without a desktop."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly fetch channel ä ") as temporary:
        path = Path(temporary) / "body 世界.bin"
        length = 4 * 1024 * 1024 + 19
        pattern = bytes((i * 37 + 11) & 255 for i in range(256))
        path.write_bytes((pattern * (length // 256 + 1))[:length])
        for mode in [
            "body",
            "stream",
            "quota",
            "scope",
            "abort",
            "abort-early",
            "reader-cancel",
            "revoke",
            "drop",
            "peerloss",
            "malformed",
            "epoch",
            "future",
            "silence",
        ]:
            result = subprocess.run(
                [str(args.probe.resolve()), mode, str(path)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=45,
            )
            assert result.returncode == 0, result
            assert (
                f"PASS actual JavaScript fetch over native process channel {mode}:" in result.stdout
            ), result
            assert (
                f"PASS main-side file ownership and reaped JavaScript child {mode}" in result.stdout
            ), result
            assert "VERIFICATION FAILED" not in result.stderr, result
            print(result.stdout.strip(), flush=True)
    print(
        "PASS actual JavaScript/native-channel component; live WebContent routing and public protocol remain open"
    )


if __name__ == "__main__":
    main()
