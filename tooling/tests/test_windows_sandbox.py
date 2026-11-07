#!/usr/bin/env python3
"""Require actual AppContainer tokens, denied native I/O and broker-owned child lifetime.

This fixture uses a static native worker, not LibWeb. Renderer integration and
its complete resource/IPC policy remain separate required acceptance.
"""

import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import time

from _windows_processes import WindowsProcesses


def line(process, timeout=20):
    """Bounded raw native pipe read; leave later report bytes for communicate()."""
    import msvcrt

    native = WindowsProcesses()
    peek = native.kernel.PeekNamedPipe
    peek.argtypes = [
        W.HANDLE,
        C.c_void_p,
        W.DWORD,
        C.POINTER(W.DWORD),
        C.POINTER(W.DWORD),
        C.POINTER(W.DWORD),
    ]
    peek.restype = W.BOOL
    pipe = msvcrt.get_osfhandle(process.stdout.fileno())
    result = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        available = W.DWORD()
        if not peek(pipe, None, 0, None, C.byref(available), None):
            raise C.WinError(C.get_last_error())
        if available.value:
            value = os.read(process.stdout.fileno(), 1)
            if value == b"\n":
                return json.loads(result)
            result.extend(value)
            assert len(result) <= 4096, "sandbox control record exceeded budget"
            continue
        assert process.poll() is None, ("broker exited before readiness", process.returncode)
        time.sleep(0.01)
    raise AssertionError("native AppContainer broker readiness timeout")


def cleanup(profile):
    # Only the exact fresh profile reported by our owned fixture may be removed.
    assert re.fullmatch(r"elytra\.firefly\.sandbox\.[0-9a-f]{32}", profile), profile
    userenv = C.WinDLL("userenv", use_last_error=True)
    userenv.DeleteAppContainerProfile.argtypes = [W.LPCWSTR]
    userenv.DeleteAppContainerProfile.restype = W.LONG
    result = userenv.DeleteAppContainerProfile(profile)
    # The normal broker removes it already; missing paths are also explicit cleanup.
    assert result >= 0 or (result & 0xFFFFFFFF) in (0x80070002, 0x80070003, 0x80070490), result


def exercise(probe, artifacts, mode):
    native = WindowsProcesses()
    with tempfile.TemporaryDirectory(prefix="firefly-sandbox-") as directory:
        root = Path(directory).resolve()
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            listener.settimeout(0.2)
            process = subprocess.Popen(
                [str(probe), str(root), str(listener.getsockname()[1]), mode],
                env={**os.environ, "FIREFLY_SANDBOX_SECRET": "must-not-enter-child"},
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            ready = None
            stdout, stderr = b"", b""
            try:
                ready = line(process)
                (artifacts / f"{mode}-launch.json").write_text(json.dumps(ready, indent=2) + "\n")
                assert ready["suspended"] is True and ready["pid"] != process.pid, ready
                assert ready["pid"] in native.children(process.pid), ready
                child = native.inspect(ready["pid"])
                expected = root / "resources ü" / "worker.exe"
                assert Path(child["image"]) == expected, child
                assert child["token"]["isAppContainer"] is True, child
                assert child["token"]["capabilityCount"] == 0, child
                assert child["token"]["integrityRid"] <= 0x1000 and child["inJob"], child
                (artifacts / f"{mode}-kernel.json").write_text(json.dumps(child, indent=2) + "\n")
                if mode != "suspended":
                    process.stdin.write(b"GO\n")
                    process.stdin.flush()
                if mode in ("hold", "suspended"):
                    if mode == "hold":
                        assert line(process) == {"running": ready["pid"]}
                    # Hold this exact native object across broker death; PID reuse
                    # cannot satisfy the kill-on-job-close assertion.
                    with native.handle(
                        native.kernel.OpenProcess(0x1000 | 0x100000, False, ready["pid"])
                    ) as handle:
                        assert native.alive(handle)
                        process.terminate()
                        stdout, stderr = process.communicate(timeout=10)
                        assert native.kernel.WaitForSingleObject(handle, 5000) == 0, (
                            "orphaned sandbox child"
                        )
                else:
                    stdout, stderr = process.communicate(timeout=20)
                    assert process.returncode == 0, (process.returncode, stdout, stderr)
                    report = json.loads(stdout)
                    assert report == {
                        "allowedRead": True,
                        "privateReadDenied": True,
                        "writeDenied": True,
                        "handleIsolated": True,
                        "cleanEnvironment": True,
                        "childSpawnDenied": True,
                        "networkDenied": True,
                    }, report
                    assert (root / "private/secret.txt").read_text() == "PRIVATE_TEST_ONLY"
                    assert not (root / "resources ü/blocked.txt").exists()
                    try:
                        unexpected, _ = listener.accept()
                    except TimeoutError:
                        pass
                    else:
                        unexpected.close()
                        raise AssertionError(
                            "AppContainer unexpectedly connected to live loopback peer"
                        )
                print("PASS native AppContainer", mode, json.dumps(ready))
            finally:
                if process.poll() is None:
                    process.terminate()
                final_stdout, final_stderr = process.communicate(timeout=10)
                (artifacts / f"{mode}-stdout.log").write_bytes(final_stdout)
                (artifacts / f"{mode}-stderr.log").write_bytes(final_stderr)
                if final_stderr:
                    print(final_stderr[:4096].decode("utf-8", errors="replace"), file=sys.stderr)
                if ready is not None:
                    cleanup(ready["profile"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--artifacts-dir", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("Native Windows AppContainer acceptance is required")
    artifacts = args.artifacts_dir.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    probe = args.probe.resolve(strict=True)
    exercise(probe, artifacts, "checks")
    exercise(probe, artifacts, "suspended")
    exercise(probe, artifacts, "hold")
    print(
        "Component proof only; LibWeb renderer/IPC/resource integration and production containment remain unfinished"
    )


if __name__ == "__main__":
    main()
