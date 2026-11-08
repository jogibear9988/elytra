#!/usr/bin/env python3
"""Reject malformed native utility frames using the actual headless helper path.

A tiny Python child duplicates only its own inherited socket to fd 3 before exec.
The test never overwrites fd 3 in the runner and never creates a desktop app.
This descriptor-inheritance fixture covers POSIX; Windows needs a native handle test.
"""

import argparse
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", required=True, type=Path)
    options = parser.parse_args()
    if os.name != "posix":
        print("SKIP utility frame boundaries: POSIX descriptor inheritance fixture")
        return
    binary = options.firefly.resolve()
    launcher = """
import os, sys
fd = int(sys.argv[1])
if fd != 3:
    os.dup2(fd, 3)
    os.close(fd)
os.set_inheritable(3, True)
os.execv(sys.argv[2], sys.argv[2:])
"""
    with tempfile.TemporaryDirectory(prefix="firefly-utility-frames-") as directory:
        entry = Path(directory) / "boundary.mjs"
        entry.write_text(
            """
console.log('BOUNDARY_ENTRY');
process.parentPort.on('error', error => console.log('BOUNDARY_ERROR', error.code));
process.parentPort.on('close', () => console.log('BOUNDARY_CLOSE'));
process.parentPort.on('message', () => console.log('UNEXPECTED_MESSAGE'));
""",
            encoding="utf-8",
        )
        cases = [
            ("oversize-length", struct.pack(">I", 128 * 1024 + 1)),
            ("truncated-header", b"\x00\x01"),
            ("truncated-payload", struct.pack(">I", 40) + b"bad"),
            ("invalid-codec", struct.pack(">I", 3) + b"bad"),
            ("empty-codec", struct.pack(">I", 0)),
        ]
        for label, wire in cases:
            parent, child = socket.socketpair()
            try:
                process = subprocess.Popen(
                    [
                        sys.executable,
                        "-c",
                        launcher,
                        str(child.fileno()),
                        str(binary),
                        "--firefly-utility",
                        str(entry),
                    ],
                    pass_fds=(child.fileno(),),
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )
                child.close()
                try:
                    parent.sendall(wire)
                    parent.shutdown(socket.SHUT_WR)
                    output, errors = process.communicate(timeout=15)
                except BaseException:
                    process.kill()
                    process.communicate()
                    raise
                assert process.returncode == 0, (label, process.returncode, output, errors)
                assert b"BOUNDARY_ENTRY" in output, (label, output, errors)
                assert b"BOUNDARY_ERROR ERR_FIREFLY_UTILITY_CHANNEL" in output, (
                    label,
                    output,
                    errors,
                )
                assert b"UNEXPECTED_MESSAGE" not in output, (label, output, errors)
                print("PASS utility frame", label)
            finally:
                parent.close()
                child.close()
        process = subprocess.run(
            [str(binary), "--firefly-utility", str(entry)],
            capture_output=True,
            timeout=15,
        )
        assert process.returncode != 0, process
        assert b"BOUNDARY_ENTRY" not in process.stdout, process
        assert b"Missing inherited utility channel" in process.stderr, process
        print("PASS utility missing fd rejected before entry")


if __name__ == "__main__":
    main()
