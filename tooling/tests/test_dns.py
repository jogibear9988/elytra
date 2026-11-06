#!/usr/bin/env python3
"""Compare OS-backed lookup and hostname TCP integration with Node."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
from pathlib import Path
import shutil
import os
import errno
import threading
import time
import tempfile
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required")
    root = Path(__file__).resolve().parents[2] / "mite/tests/dns"
    failures = 0
    for name in ["lookup.mjs", "net.mjs", "custom.mjs", "cancel.mjs", "refused.mjs", "errors.mjs"]:
        try:
            results = [
                subprocess.run([exe, str(root / name)], capture_output=True, text=True, timeout=20)
                for exe in [args.node, args.mite]
            ]
            passed = (
                all(r.returncode == 0 and not r.stderr for r in results)
                and results[0].stdout == results[1].stdout
            )
        except subprocess.TimeoutExpired as e:
            print(f"FAIL {name}: timeout {e.stdout!r} {e.stderr!r}")
            failures += 1
            continue
        print(f"{'PASS' if passed else 'FAIL'} dns/{name}")
        if not passed:
            failures += 1
            print("Node:", results[0])
            print("Mite:", results[1])
    results = [
        subprocess.run(
            [exe, str(root / "null-byte.mjs")], capture_output=True, text=True, timeout=10
        )
        for exe in [args.node, args.mite]
    ]
    expected = [
        '{"address":"127.0.0.1","family":4}\n',
        '{"code":"ENOTFOUND","syscall":"getaddrinfo","hostname":"127.0.0.1\\u0000ignored"}\n',
    ]
    passed = all(
        r.returncode == 0 and not r.stderr and r.stdout == e for r, e in zip(results, expected)
    )
    print(f"{'PASS' if passed else 'FAIL'} recorded DNS embedded-NUL rejection difference")
    if not passed:
        print(results)
    failures += int(not passed)
    for name, message in [
        ("fatal.cjs", "resolver callback failed"),
        ("pending-fatal.cjs", "fatal with pending resolver"),
    ]:
        for exe in [args.node, args.mite]:
            result = subprocess.run(
                [exe, str(root / name)], capture_output=True, text=True, timeout=10
            )
            passed = result.returncode == 1 and message in result.stderr and not result.stdout
            print(f"{'PASS' if passed else 'FAIL'} {name} ({Path(exe).name})")
            failures += int(not passed)
    if os.name != "nt":
        # Occupy the native libuv pool before submitting getaddrinfo. The timer
        # must still publish readiness, then independent FIFO writers release it.
        for exe in [args.node, args.mite]:
            with tempfile.TemporaryDirectory(prefix="mite-dns-workers-") as directory:
                fifos = [Path(directory) / f"fifo-{i}" for i in range(4)]
                for fifo in fifos:
                    os.mkfifo(fifo)
                process = subprocess.Popen(
                    [exe, str(root / "worker-queue.mjs"), *map(str, fifos)],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    env={**os.environ, "UV_THREADPOOL_SIZE": "4"},
                )
                watchdog = threading.Timer(15, process.kill)
                watchdog.start()
                passed = False
                try:
                    ready = process.stdout.readline()
                    if ready != "timer false\n":
                        raise ValueError(f"Timer did not run: {ready!r}")
                    for fifo in fifos:
                        deadline = time.monotonic() + 3
                        while True:
                            try:
                                fd = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
                                break
                            except OSError as e:
                                if e.errno not in (errno.ENXIO, errno.EAGAIN):
                                    raise
                                if time.monotonic() > deadline:
                                    raise
                                time.sleep(0.01)
                        try:
                            os.write(fd, b"x")
                        finally:
                            os.close(fd)
                    stdout, stderr = process.communicate(timeout=5)
                    passed = (
                        process.returncode == 0
                        and not stderr
                        and stdout == "lookup true 127.0.0.1 4 true\n"
                    )
                    if not passed:
                        print(stdout, stderr, process.returncode)
                except (OSError, ValueError, subprocess.TimeoutExpired) as e:
                    print(e)
                finally:
                    watchdog.cancel()
                    if process.poll() is None:
                        process.kill()
                    process.communicate()
                print(
                    f"{'PASS' if passed else 'FAIL'} queued OS resolver leaves loop responsive ({Path(exe).name})"
                )
                failures += int(not passed)
    else:
        print("UNVERIFIED: POSIX FIFO worker-queue scenario on native Windows")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
