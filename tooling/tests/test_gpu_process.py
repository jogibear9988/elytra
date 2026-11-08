#!/usr/bin/env python3
"""One explicit offscreen owned-compositor termination batch; never fall back to desktop."""

import _support

import argparse
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
from test_renderer import ROOT, environment, stop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "darwin" and not sys.platform.startswith("linux"):
        parser.error("Requires actual macOS/Linux; no Windows process acceptance is claimed")
    with tempfile.TemporaryDirectory(prefix="elytra-owned-gpu-") as temporary:
        root = Path(temporary)
        (root / "config").mkdir()
        for name in ["process.mjs", "owned.html"]:
            shutil.copyfile(ROOT / "firefly/tests/gpu" / name, root / name)
        fixture_env = environment(str(root / "config"))
        fixture_env["QT_QPA_PLATFORM"] = "offscreen"
        fixture_env["LIBGL_ALWAYS_SOFTWARE"] = "1"
        command = [str(args.firefly.resolve()), str(root / "process.mjs"), "--force-cpu-painting"]
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=fixture_env,
            start_new_session=os.name != "nt",
        )
        try:
            stdout, stderr = process.communicate(timeout=25)
            assert process.returncode == 0 and "PASS real owned compositor SIGKILL" in stdout, (
                command,
                process.returncode,
                stdout,
                stderr,
            )
            assert "VERIFICATION FAILED" not in stderr, stderr
            print(stdout.strip(), flush=True)
            print(
                "PASS explicit offscreen real owned-PID lifecycle only; no visible window or GPU-device acceptance"
            )
        finally:
            stop(process)
            process.communicate()


if __name__ == "__main__":
    main()
