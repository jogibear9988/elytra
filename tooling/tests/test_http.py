#!/usr/bin/env python3
"""Compare actual HTTP server behavior over TCP with Node."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required")
    root = Path(__file__).resolve().parents[2] / "mite/tests/http"
    failures = 0
    for name in [
        "basic.mjs",
        "body.mjs",
        "pipeline.mjs",
        "head.mjs",
        "continue.mjs",
        "errors.mjs",
        "keep-alive.mjs",
        "half-close.mjs",
        "large.mjs",
        "default-errors.mjs",
        "http10.mjs",
    ]:
        try:
            results = [
                subprocess.run([exe, str(root / name)], capture_output=True, text=True, timeout=15)
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
        print(f"{'PASS' if passed else 'FAIL'} http/{name}")
        if not passed:
            failures += 1
            print("Node:", results[0])
            print("Mite:", results[1])
    for exe in [args.node, args.mite]:
        result = subprocess.run(
            [exe, str(root / "fatal.mjs")], capture_output=True, text=True, timeout=10
        )
        passed = (
            result.returncode == 1
            and "HTTP request callback failed" in result.stderr
            and not result.stdout
        )
        print(f"{'PASS' if passed else 'FAIL'} fatal HTTP request callback ({Path(exe).name})")
        failures += int(not passed)
    result = subprocess.run(
        [args.mite, str(root / "policy.mjs")], capture_output=True, text=True, timeout=10
    )
    expected = (
        "headers timeout HTTP/1.1 408 Request Timeout\nbody timeout HTTP/1.1 408 Request Timeout\n"
        "upgrade HTTP/1.1 501 Not Implemented\ntunnel HTTP/1.1 501 Not Implemented\n"
        + "ERR_MITE_UNSUPPORTED_API\n"
        * 4
    )
    passed = result.returncode == 0 and not result.stderr and result.stdout == expected
    print(f"{'PASS' if passed else 'FAIL'} explicit HTTP timeout/unsupported policies (Mite)")
    if not passed:
        print(result)
    failures += int(not passed)
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
