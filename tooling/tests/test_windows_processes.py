#!/usr/bin/env python3
"""Check real read-only Windows process observations with an owned native Python child."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch

from _windows_processes import WindowsProcesses
from _windows_window import loaded_renderer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-guard", action="store_true")
    args = parser.parse_args()
    if args.check_guard:
        with patch("sys.platform", "darwin"):
            try:
                WindowsProcesses()
            except RuntimeError as error:
                assert "require Windows" in str(error), error
            else:
                raise AssertionError("Non-Windows native observations were accepted")
        print("PASS unsupported-host refusal before loading any native Windows API")
        return
    native = WindowsProcesses()
    parent = native.inspect(os.getpid())
    assert Path(parent["image"]) == Path(sys.executable).resolve(strict=True), parent
    child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
    try:
        value = native.inspect(child.pid)
        assert child.poll() is None and child.pid in native.children(os.getpid()), value
        assert value["pid"] == child.pid and Path(value["image"]) == Path(sys.executable).resolve(
            strict=True
        ), value
        assert value["creationTicks"] >= parent["creationTicks"] > 0, (value, parent)
        assert type(value["inJob"]) is bool, value
        assert type(value["token"]["integrityRid"]) is int, value
        for key in ("isAppContainer", "hasRestrictions", "restrictedSids"):
            assert type(value["token"][key]) is bool, value
        print(json.dumps({"parent": parent, "child": value}, indent=2))
    finally:
        child.terminate()
        child.communicate(timeout=10)
    for invalid in (0, -1, 2**32, True, "1"):
        try:
            native.inspect(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError(f"Invalid PID accepted: {invalid!r}")
    try:
        native.inspect(child.pid)
    except ProcessLookupError:
        pass
    else:
        raise AssertionError("Exited owned child was reported as live")
    print(
        "PASS native Windows process/parent/image/creation/token/job observations and invalid/exited PID refusal"
    )
    # Exercise only the native pipe protocol with an owned Python peer. The
    # separate LibWeb/window gate supplies the actual WebContent implementation.
    source = "import os,sys,time; print('protocol startup',flush=True); print('WebContent PID: '+str(os.getpid()),flush=True); time.sleep(30)"
    peer = subprocess.Popen([sys.executable, "-c", source], stdout=subprocess.PIPE)
    try:
        assert loaded_renderer(peer) == peer.pid
    finally:
        peer.terminate()
        peer.communicate(timeout=10)
    silent = subprocess.Popen(
        [sys.executable, "-c", "import time; time.sleep(30)"], stdout=subprocess.PIPE
    )
    try:
        try:
            loaded_renderer(silent, timeout=0.1)
        except AssertionError as error:
            assert "did not report" in str(error), error
        else:
            raise AssertionError("Silent peer reported a fabricated renderer PID")
    finally:
        silent.terminate()
        silent.communicate(timeout=10)
    noisy = subprocess.Popen(
        [
            sys.executable,
            "-c",
            "import sys,time; sys.stdout.buffer.write(b'x'*(1024*1024+1)); sys.stdout.flush(); time.sleep(30)",
        ],
        stdout=subprocess.PIPE,
    )
    try:
        try:
            loaded_renderer(noisy)
        except AssertionError as error:
            assert "one-MiB budget" in str(error), error
        else:
            raise AssertionError("Oversized startup was accepted")
    finally:
        noisy.terminate()
        noisy.communicate(timeout=10)
    print(
        "PASS actual Windows pipe readiness, timeout and startup budget without selectors or reader-thread leaks"
    )
    print("Component test only; this native Python peer is not renderer or sandbox acceptance")


if __name__ == "__main__":
    main()
