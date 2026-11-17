#!/usr/bin/env python3
"""Compare actual borrowed-fd Writable output and EOF with Node through OS pipes."""

import _support  # noqa: F401 — standalone regression imports.

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import threading
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]


def slow_pipe(binary):
    source = ROOT / "mite/tests/process-stdio/backpressure.mjs"
    output = bytearray()
    errors = bytearray()
    failures = []

    def drain(stream, target, slow):
        try:
            while chunk := stream.read(4096):
                target.extend(chunk)
                if len(target) > 3 * 1024 * 1024:
                    raise AssertionError("Unexpected unbounded fixture output")
                if slow:
                    time.sleep(0.002)
        except Exception as error:
            failures.append(error)

    with subprocess.Popen(
        [str(binary), str(source)], stdout=subprocess.PIPE, stderr=subprocess.PIPE
    ) as process:
        readers = [
            threading.Thread(target=drain, args=(process.stdout, output, True)),
            threading.Thread(target=drain, args=(process.stderr, errors, False)),
        ]
        for reader in readers:
            reader.start()
        try:
            process.wait(timeout=20)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3)
            raise
        finally:
            for reader in readers:
                reader.join(timeout=3)
        assert all(not reader.is_alive() for reader in readers), "Owned pipe reader did not stop"
        assert not failures, failures
        assert process.returncode == 0, (binary, process.returncode, bytes(errors))
    expected = b"".join(
        bytes((offset * 17 + piece) & 255 for offset in range(65536 + piece * 13))
        for piece in range(32)
    )
    assert output == expected, (
        binary,
        len(output),
        len(expected),
        "Short-write suffix/FIFO mismatch",
    )
    assert errors == b"PASS native pipe backpressure callbacks/FIFO/drain\n", bytes(errors)


def redirected_files(binary):
    for name, aliases in [("small-unref", False), ("aliases", True)]:
        with tempfile.TemporaryFile() as output, tempfile.TemporaryFile() as errors:
            result = subprocess.run(
                [str(binary), str(ROOT / "mite/tests/process-stdio" / f"{name}.mjs")],
                stdout=output,
                stderr=output if aliases else errors,
                timeout=10,
            )
            assert result.returncode == 0, (binary, name, result.returncode)
            output.seek(0)
            errors.seek(0)
            expected = (
                b"aliased stdout\naliased stderr\n" if aliases else b"small native pipe output\n"
            )
            assert output.read(4096) == expected and not errors.read(4096), (binary, name)


def closed_pipe(binary):
    read, write = os.pipe()
    try:
        with subprocess.Popen(
            [str(binary), str(ROOT / "mite/tests/process-stdio/epipe.mjs")],
            stdin=subprocess.PIPE,
            stdout=write,
            stderr=subprocess.PIPE,
        ) as process:
            os.close(write)
            write = None
            os.close(read)
            read = None
            # The child's readSync cannot complete until no reader exists.
            try:
                _, errors = process.communicate(input=b"g", timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate(timeout=3)
                raise
            assert (
                process.returncode == 0
                and errors == b"PASS native closed pipe EPIPE callback/error identity\n"
            ), (binary, process.returncode, errors)
    finally:
        if read is not None:
            os.close(read)
        if write is not None:
            os.close(write)


def borrowed_flags(binary, *, host=False, aliases=False):
    if os.name == "nt":
        return False
    import fcntl

    read, write = os.pipe()
    try:
        # XNU adds FWASWRITTEN on the first successful write. Prime and drain
        # this owned OFD before snapshotting, so the full comparison measures
        # restoration rather than a kernel-maintained first-write transition.
        assert os.write(write, b"p") == 1 and os.read(read, 1) == b"p"
        original = fcntl.fcntl(write, fcntl.F_GETFL)
        name = "aliases.mjs" if aliases else "small-unref.mjs"
        result = subprocess.run(
            [str(binary), str(ROOT / "mite/tests/process-stdio" / name)],
            stdout=write,
            stderr=write if aliases else subprocess.PIPE,
            timeout=10,
        )
        expected_error = None if aliases else (b"HOST_SHUTDOWN\n" if host else b"")
        assert result.returncode == 0 and result.stderr == expected_error, result
        # dup2 into the child shares this open file description: a lingering
        # O_NONBLOCK change survives child exit and is independently observable.
        assert fcntl.fcntl(write, fcntl.F_GETFL) == original, (
            "Borrowed pipe flags were not restored"
        )
        os.write(write, b"parent descriptor alive\n")
        os.close(write)
        write = None
        output = bytearray()
        while chunk := os.read(read, 4096):
            output.extend(chunk)
        expected = b"aliased stdout\naliased stderr\n" if aliases else b"small native pipe output\n"
        if aliases and host:
            expected += b"HOST_SHUTDOWN\n"
        assert output == expected + b"parent descriptor alive\n", output
    finally:
        os.close(read)
        if write is not None:
            os.close(write)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument(
        "--host",
        type=Path,
        help="Optional genuine embedded runtime host for descriptor restoration",
    )
    args = parser.parse_args()
    if not args.node:
        parser.error("A real Node reference is required")
    expected = {
        "raw": (bytes(range(256)) * 256 + "Grüße 🦋\n".encode(), "native stderr 🦋\n".encode()),
        "partial": ("partial Grüße 🦋\nafter end/destroy\n".encode(), b"stderr descriptor alive\n"),
        "small-unref": (b"small native pipe output\n", b""),
    }
    for name, (out, err) in expected.items():
        source = ROOT / "mite/tests/process-stdio" / f"{name}.mjs"
        for binary in [args.node, args.mite]:
            result = subprocess.run([binary, str(source)], capture_output=True, timeout=15)
            assert result.returncode == 0, (binary, name, result)
            assert result.stdout == out and result.stderr == err, (binary, name, result)
        print(f"PASS process stdio {name}: actual Node/Mite pipe bytes, callbacks and EOF")
    for binary in [args.node, args.mite]:
        redirected_files(binary)
        closed_pipe(binary)
        slow_pipe(binary)
    print("PASS process stdio: regular file sync/unref output and gated real EPIPE callback/error")
    print(
        "PASS process stdio: independent slow pipe drain preserves exact suffix/FIFO and backpressure"
    )
    if borrowed_flags(args.node) and borrowed_flags(args.node, aliases=True):
        print(
            "PASS Node reference: full borrowed pipe flags restored for separate/aliased descriptors"
        )
    if borrowed_flags(args.mite):
        print(
            "PASS native process stdio: borrowed open-file flags restored and parent descriptor remains alive"
        )

    if borrowed_flags(args.mite, aliases=True):
        print(
            "PASS native process stdio: stdout/stderr alias FIFO and shared borrowed flag restoration"
        )
    if args.host:
        if borrowed_flags(args.host.resolve(), host=True):
            print(
                "PASS embedded process stdio: borrowed descriptor flags restored at owner teardown"
            )
        if borrowed_flags(args.host.resolve(), host=True, aliases=True):
            print(
                "PASS embedded process stdio: aliased stdout/stderr/shutdown FIFO and shared flags"
            )


if __name__ == "__main__":
    main()
