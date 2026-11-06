#!/usr/bin/env python3
"""Exercise the real LibWebView probe; a missing/unbuilt probe is a failure."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
from pathlib import Path
import os
import signal
import re
import tempfile
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]


def stop(process):
    if os.name == "nt":
        if process.poll() is None:
            process.kill()
    else:
        # The leader can have exited while a helper still holds stdout open.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass


def environment(directory):
    return {
        **os.environ,
        "XDG_CONFIG_HOME": directory,
        "XDG_DATA_HOME": directory,
        "XDG_CACHE_HOME": directory,
    }


def run(probe, *args):
    command = [str(probe), *map(str, args)]
    with tempfile.TemporaryDirectory(prefix="firefly-probe-env-") as directory:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=environment(directory),
            start_new_session=os.name != "nt",
        )
        try:
            stdout, stderr = process.communicate(timeout=45)
            result = subprocess.CompletedProcess(command, process.returncode, stdout, stderr)
            result.pid = process.pid
            return result
        finally:
            stop(process)
            process.communicate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--artifacts-dir", type=Path, default=ROOT / "build/renderer-artifacts")
    args = parser.parse_args()
    probe = args.probe.resolve()
    if not probe.is_file():
        parser.error("Build the renderer probe first using tooling/build_renderer.py")
    page = (ROOT / "firefly/tests/renderer/page.html").as_uri()
    text = run(probe, "--headless=text", page)
    expected = [
        "Firefly renderer",
        "LIBJS_DOM_OK",
        "CAPABILITIES_ABSENT=true",
        "NODE_IMPORT_DENIED=true",
    ]
    renderer_pid = re.search(r"^WebContent PID: (\d+)$", text.stdout, re.MULTILINE)
    if (
        text.returncode
        or not renderer_pid
        or int(renderer_pid[1]) <= 0
        or int(renderer_pid[1]) == text.pid
        or not all(value in text.stdout for value in expected)
    ):
        print(text)
        return 1
    print("PASS real LibWeb text + LibJS DOM + absent Mite capabilities")
    directory = args.artifacts_dir.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    screenshot = directory / "page.png"
    screenshot.unlink(missing_ok=True)
    result = run(
        probe,
        "--headless=screenshot",
        "--screenshot-delay=2",
        "--window-width=640",
        "--window-height=400",
        f"--screenshot-path={screenshot}",
        page,
    )
    if result.returncode or not screenshot.is_file():
        print(result)
        return 1
    data = screenshot.read_bytes()
    if (
        len(data) < 33
        or data[:8] != b"\x89PNG\r\n\x1a\n"
        or struct.unpack(">II", data[16:24]) != (640, 400)
    ):
        print("FAIL renderer screenshot dimensions/content header")
        return 1
    print("PASS real 640x400 compositor screenshot")
    if os.name != "nt":
        with tempfile.TemporaryDirectory(prefix="firefly-probe-env-") as directory:
            process = subprocess.Popen(
                [
                    str(probe),
                    "--headless=manual",
                    (ROOT / "firefly/tests/renderer/processes.html").as_uri(),
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                env=environment(directory),
                start_new_session=True,
            )
            found = set()
            children = {}
            try:
                deadline = time.monotonic() + 20
                while process.poll() is None and time.monotonic() < deadline:
                    rows = subprocess.check_output(
                        ["ps", "-axo", "pid=,ppid=,comm="], text=True
                    ).splitlines()
                    for row in rows:
                        pid, ppid, command = row.strip().split(maxsplit=2)
                        if int(ppid) == process.pid and int(pid) != process.pid:
                            name = Path(command).name
                            found.add(name)
                            children.setdefault(name, set()).add(int(pid))
                    # A spare WebContent can precede the actual navigated renderer.
                    # Observe the full session so the reported PID is checked too.
                    time.sleep(0.1)
                stdout, stderr = process.communicate(timeout=15)
                renderer_pid = re.search(r"^WebContent PID: (\d+)$", stdout, re.MULTILINE)
                if (
                    process.returncode
                    or not renderer_pid
                    or int(renderer_pid[1]) not in children.get("WebContent", set())
                    or not {"WebContent", "RequestServer", "Compositor"} <= found
                ):
                    print(
                        "FAIL helper process isolation", found, process.returncode, stdout, stderr
                    )
                    return 1
                print("PASS separate WebContent/RequestServer/Compositor OS processes")
            finally:
                stop(process)
                process.communicate()
    else:
        print("UNVERIFIED: native Windows process/sandbox validation remains required")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
