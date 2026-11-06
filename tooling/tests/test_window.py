#!/usr/bin/env python3
"""Run the native macOS window gate; no headless or missing-binary fallback."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import os
from pathlib import Path
import re
import selectors
import signal
import struct
import subprocess
import sys
import tempfile
import time

from test_renderer import ROOT, environment, run, stop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("Native Windows/Linux window adapters and tests remain unverified")
    probe = args.probe.resolve()
    if not probe.is_file():
        parser.error("Build with tooling/build_renderer.py --target firefly-window-probe")
    capture = ROOT / "build/renderer-artifacts/window.png"
    capture.parent.mkdir(parents=True, exist_ok=True)
    capture.unlink(missing_ok=True)
    page = (ROOT / "firefly/tests/renderer/window.html").as_uri()
    result = run(probe, "--probe-self-test", f"--probe-capture={capture}", page)
    expected = [
        "PASS native Qt mouse/key input and viewport resize",
        "PASS native view navigation and window close",
    ]
    if (
        result.returncode
        or not all(marker in result.stdout for marker in expected)
        or not capture.is_file()
    ):
        print(result)
        return 1
    data = capture.read_bytes()
    if len(data) < 33 or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError("Missing native PNG capture")
    width, height = struct.unpack(">II", data[16:24])
    if width < 800 or height < 500 or width * 5 != height * 8:
        raise AssertionError(f"Wrong native viewport capture: {width}x{height}")
    print("PASS native window, Qt mouse/key events, resize, navigation, close and capture")

    with tempfile.TemporaryDirectory(prefix="firefly-window-env-") as directory:
        process = subprocess.Popen(
            [str(probe), page],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=environment(directory),
            start_new_session=True,
        )
        try:
            deadline = time.monotonic() + 15
            renderer_pid = None
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ)
                while process.poll() is None and time.monotonic() < deadline:
                    if not selector.select(0.1):
                        continue
                    line = process.stdout.readline()
                    match = re.fullmatch(r"WebContent PID: (\d+)\n", line)
                    if match:
                        renderer_pid = int(match[1])
                        break
            if renderer_pid is None:
                raise AssertionError("No loaded renderer to crash")
            rows = subprocess.check_output(
                ["ps", "-axo", "pid=,ppid=,comm="], text=True
            ).splitlines()
            children = [row.strip().split(maxsplit=2) for row in rows]
            if not any(
                int(pid) == renderer_pid
                and int(ppid) == process.pid
                and Path(command).name == "WebContent"
                for pid, ppid, command in children
            ):
                raise AssertionError("Reported renderer is not an owned WebContent child")
            os.kill(renderer_pid, signal.SIGKILL)
            stdout, stderr = process.communicate(timeout=15)
            if process.returncode != 1 or "Firefly WebContent crashed" not in stderr:
                raise AssertionError(
                    f"Crash was not reported as failure: {process.returncode}, {stdout}, {stderr}"
                )
            print("PASS actual WebContent process crash reports failure and exits")
        finally:
            stop(process)
            process.communicate()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
