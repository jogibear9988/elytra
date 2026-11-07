#!/usr/bin/env python3
"""Prove wire data survives complete VM/process teardown, including bad remote input."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    args = parser.parse_args()
    command = str(args.probe.resolve())
    checks = subprocess.run([command, "checks"], capture_output=True, timeout=30, check=True)
    assert b"PASS wire graph round trip" in checks.stdout, checks
    print(checks.stdout.decode().strip())
    encoded = subprocess.run(
        [command, "encode"], capture_output=True, timeout=15, check=True
    ).stdout
    assert encoded and len(encoded) <= 1024 * 1024
    # run() waited for the producer to exit: no VM/heap references can survive it.
    decoded = subprocess.run(
        [command, "decode"], input=encoded, capture_output=True, timeout=15, check=True
    ).stdout
    assert decoded == encoded, "Fresh VM data/re-capture changed graph semantics"
    for bad in [
        b"",
        b"{}",
        b"[1,[[7,0]]]",
        b"[1,[[5,false,[[[],99,true]]]]]",
        b"[" * 9 + b"0" + b"]" * 9,
        b" " * (1024 * 1024 + 1),
    ]:
        result = subprocess.run([command, "decode"], input=bad, capture_output=True, timeout=15)
        assert result.returncode == 1 and not result.stdout, (
            result.returncode,
            result.stdout,
            result.stderr,
        )
    assert (
        subprocess.run(
            [command, "decode"], input=encoded, capture_output=True, timeout=15, check=True
        ).stdout
        == encoded
    )
    # Independent Python peer: binary payload is exactly the original bytes,
    # rather than JSON numbers/base64 or a surviving producer VM allocation.
    binary = subprocess.run(
        [command, "binary-encode"], capture_output=True, timeout=15, check=True
    ).stdout
    assert binary[:4] == b"FFB2" and len(binary) < 65536 + 1024
    metadata_length = int.from_bytes(binary[4:8], "little")
    metadata = json.loads(binary[8 : 8 + metadata_length])
    payload = binary[8 + metadata_length :]
    expected = bytes((i * 37 + 11) & 255 for i in range(65536))
    assert metadata[0] == 2 and payload == expected
    assert sum(row[1] for row in metadata[1] if row[0] == 8) == len(expected)
    assert (
        subprocess.run(
            [command, "binary-decode"], input=binary, capture_output=True, timeout=15, check=True
        ).stdout
        == binary
    )

    def frame(nodes, raw=expected, version=2):
        metadata = json.dumps([version, nodes], separators=(",", ":")).encode()
        return b"FFB2" + len(metadata).to_bytes(4, "little") + metadata + raw

    def prop(name, index):
        return [[ord(c) for c in name], index, True]

    # Handwritten graph from a different implementation must decode in a fresh
    # process with aliases, cycles and exact arbitrary bytes intact.
    nodes = [
        [5, False, [prop("buffer", 1), prop("alias", 1), prop("empty", 2), prop("self", 0)]],
        [8, len(expected)],
        [8, 0],
    ]
    independent = frame(nodes)
    assert (
        subprocess.run(
            [command, "binary-decode"],
            input=independent,
            capture_output=True,
            timeout=15,
            check=True,
        ).stdout
        == independent
    )
    for bad in [
        binary[:7],
        binary[:-1],
        binary + b"x",
        b"FFB2\xff\xff\xff\xff",
        frame([[8, -1]]),
        frame([[8, 0.5]]),
        frame([[8, 65537]]),
        frame([[7, 0]]),
        frame([[9, 0]]),
        frame([[8, 0]], b"x"),
        frame([[8, 0], [8, 0]], b""),
        frame([[0]], b""),
        frame([[8, 0]], b"", version=1),
    ]:
        result = subprocess.run(
            [command, "binary-decode"], input=bad, capture_output=True, timeout=15
        )
        assert result.returncode == 1 and not result.stdout, (result.returncode, result.stdout)
    # Existing public/default decoding still refuses the opt-in internal format.
    result = subprocess.run([command, "decode"], input=binary, capture_output=True, timeout=15)
    assert result.returncode == 1 and not result.stdout
    print(
        "PASS internal raw binary transfer after producer exit, independent Python frame/bytes and default-policy refusal"
    )
    print(
        "PASS separate process/VM wire transfer after producer exit, fresh prototypes/GC/cycles/UTF-16/IEEE values and bounded malformed-input rejection"
    )


if __name__ == "__main__":
    main()
