#!/usr/bin/env python3
"""Exercise the real two-process URL request/file response service without a GUI."""

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    probe = args.probe.resolve()

    def packet(kind=1, epoch=101, request=7, credit=0, url=b"app://bundle/body"):
        return (
            struct.pack("<4sBBHQIIQ", b"FFQP", 1, kind, 0, epoch, request, len(url), credit) + url
        )

    def decode(wire, expected):
        result = subprocess.run(
            [str(probe), "request-decode", "-"], input=wire, capture_output=True, timeout=10
        )
        assert (result.returncode == 0) == expected, result
        return result

    encoded = subprocess.run([str(probe), "request-encode", "-"], capture_output=True, timeout=10)
    assert encoded.returncode == 0, encoded
    assert encoded.stdout == packet(epoch=0x0102030405060708, request=0x10203040)
    for valid in [
        packet(),
        packet(kind=2),
        packet(kind=3, credit=65536, url=b""),
        packet(kind=4, url=b""),
        packet(url="app://bundle/世界".encode()),
    ]:
        assert decode(valid, True).stdout.strip().split()[1:3] == [b"101", b"7"]
    for invalid in [
        b"",
        packet()[:-1],
        packet() + b"x",
        packet(kind=0),
        packet(kind=5),
        packet(epoch=0),
        packet(request=0),
        packet(credit=1),
        packet(url=b""),
        packet(url=b"a" * 8193),
        packet(url=b"\xff"),
        packet(url=b"a\0b"),
        packet(kind=3, credit=0, url=b""),
        packet(kind=3, credit=65537, url=b""),
        packet(kind=3, credit=1),
        packet(kind=4, credit=1, url=b""),
    ]:
        decode(invalid, False)
    for offset in [0, 4, 6, 7, 20, 23]:
        invalid = bytearray(packet())
        invalid[offset] ^= 255
        decode(invalid, False)
    print("PASS independent request framing, identity, URL UTF-8/size and credit policy controls")

    with tempfile.TemporaryDirectory(prefix="firefly service ä ") as temporary:
        path = Path(temporary) / "body 世界.bin"
        length = 4 * 1024 * 1024 + 19
        pattern = bytes((i * 37 + 11) & 255 for i in range(256))
        path.write_bytes((pattern * (length // 256 + 1))[:length])
        for mode in [
            "body",
            "multiplex",
            "cancel",
            "peerloss",
            "kill",
            "revoke",
            "reentrant",
            "malformed",
            "epoch",
            "replay",
            "overcredit",
            "future-credit",
            "quota",
            "deadline",
            "async-body",
            "async-duplicate",
            "async-order",
            "async-cancel",
            "async-revoke",
            "async-owner",
            "async-kill",
            "async-quota",
            "async-deadline",
        ]:
            result = subprocess.run(
                [str(probe), mode, str(path)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=50,
            )
            assert result.returncode == 0, result
            assert f"PASS native resource service {mode}:" in result.stdout, result
            assert "VERIFICATION FAILED" not in result.stderr, result
            print(result.stdout.strip(), flush=True)
    print(
        "PASS real separate-process resource service; broader protocol/session compatibility remains partial"
    )


if __name__ == "__main__":
    main()
