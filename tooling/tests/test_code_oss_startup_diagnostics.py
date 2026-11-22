#!/usr/bin/env python3
"""Verify that startup diagnostics cannot publish arbitrary app output."""

from pathlib import Path
import tempfile

from test_code_oss_workbench import MAX_LOG, startup_checkpoints


def main():
    with tempfile.TemporaryDirectory(prefix="ef-startup-controls-") as directory:
        raw = Path(directory) / "run.private.log"
        startup = (
            b'Code-OSS settings: {"environment":"fixture-private"}\n'
            b"1.234 WebContent(71): FIREFLY_NAVIGATION_CHANNEL_READY pid=71 epoch=1\n"
            b"FIREFLY_PROTOCOL_DIAGNOSTIC stage=server-admit count=1 errno=0\n"
            b"FIREFLY_PROTOCOL_DIAGNOSTIC stage=server-admit count=2 errno=0\n"
            b"FIREFLY_PROTOCOL_DIAGNOSTIC stage=fixture-private count=2 errno=0\n"
            b"FIREFLY_PROTOCOL_DIAGNOSTIC stage=server-packet count=2 errno=0 fixture-private\n"
            b"FIREFLY_DOCUMENT_CHANNEL_READY pid=71 epoch="
        )
        raw.write_bytes(
            startup + b"2\nFIREFLY_PROTOCOL_DIAGNOSTIC stage=client-revoke count=1 errno=0\n"
        )
        assert startup_checkpoints(raw, len(startup)) == {
            "checkpoints": {"navigation-channel-ready": 1, "server-admit": 2},
            "truncated": False,
        }
        with raw.open("r+b") as output:
            output.seek(MAX_LOG)
            output.write(b"\nFIREFLY_PROTOCOL_DIAGNOSTIC stage=client-deadline count=1 errno=0\n")
        bounded = startup_checkpoints(raw, raw.stat().st_size)
        assert bounded["truncated"] and "client-deadline" not in bounded["checkpoints"]
    print("PASS Code-OSS diagnostic allowlist, partial-line cutoff and bounded reads")


if __name__ == "__main__":
    main()
