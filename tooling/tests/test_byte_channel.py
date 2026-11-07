#!/usr/bin/env python3
"""Validate native byte transport/credit/lifetime and a separately bootstrapped OS process.

This component does not establish a WebContent/preload/document-bound public API.
"""

import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    command = str(args.probe.resolve(strict=True))
    checks = subprocess.run([command, "checks"], capture_output=True, timeout=45, check=True)
    for phrase in (
        b"PASS native packet raw bytes",
        b"PASS actual native channel order",
        b"PASS native unsolicited ACK and replay",
        b"PASS native receive count bound",
        b"PASS native dispatch reentry",
        b"PASS maximum native payload",
    ):
        assert phrase in checks.stdout, checks
    print(checks.stdout.decode().strip())
    expected = bytes((i * 37 + 11) & 255 for i in range(128 * 1024))
    # Independent, fixed little-endian protocol peer, including the reserved bytes.
    header = (
        b"FFBC"
        + bytes([1, 0, 0, 0])
        + (17).to_bytes(8, "little")
        + len(expected).to_bytes(4, "little")
    )
    independent = header + expected
    encoded = subprocess.run(
        [command, "packet-encode"], capture_output=True, timeout=15, check=True
    )
    assert encoded.stdout == independent, "Native packet bytes differed from independent format"
    decoded = subprocess.run(
        [command, "packet-decode"], input=independent, capture_output=True, timeout=15, check=True
    )
    assert b"PASS independent native packet" in decoded.stdout, decoded
    for invalid in (
        b"",
        independent[:19],
        independent[:-1],
        independent + b"x",
        b"BAD!" + independent[4:],
        independent[:4] + b"\x02" + independent[5:],
        independent[:6] + b"\x01" + independent[7:],
    ):
        result = subprocess.run(
            [command, "packet-decode"], input=invalid, capture_output=True, timeout=15
        )
        assert result.returncode == 1 and not result.stdout, result
    print("PASS independent native packet format, arbitrary bytes and malformed input refusal")
    process = subprocess.Popen([command, "process"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        stdout, stderr = process.communicate(timeout=20)
        assert process.returncode == 0, (process.returncode, stdout, stderr)
        records = [json.loads(line) for line in stdout.splitlines() if line.startswith(b"{")]
        assert len(records) == 1, stdout
        record = records[0]
        assert (
            record["parent"] == process.pid
            and record["child"] > 0
            and record["child"] != process.pid
        ), record
        assert record["bytes"] == 65536, record
        assert b"PASS actual native child byte round trip" in stdout, stdout
        print(stdout.decode().strip())
    finally:
        if process.poll() is None:
            process.kill()
        process.communicate(timeout=10)
    print(
        "Component proof only; actual WebContent handoff, document authorization and public binary IPC remain required"
    )


if __name__ == "__main__":
    main()
