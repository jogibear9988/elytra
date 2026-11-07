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
    print("Component test only; this native Python peer is not renderer or sandbox acceptance")


if __name__ == "__main__":
    main()
