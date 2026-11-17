#!/usr/bin/env python3
"""One offscreen batch of real dedicated module Worker fetch ownership checks."""

import _support
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from test_renderer import ROOT, environment, stop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    markers = [
        "PASS dedicated Worker custom module and HTTP policy cancellation",
        "PASS dedicated Worker actual header decision and mutation",
        "PASS nested dedicated Worker custom module HTTP and headers",
        "PASS parent Worker termination revokes nested pending fetch",
        "PASS dedicated Worker termination revokes pending fetch",
        "PASS dedicated Worker owner navigation revokes pending fetch",
    ]
    with tempfile.TemporaryDirectory(prefix="elytra-worker-fetch-") as temporary:
        root = Path(temporary)
        (root / "config").mkdir()
        for name in ["main.mjs", "page.mjs", "page.html"]:
            shutil.copyfile(ROOT / "firefly/tests/worker-fetch" / name, root / name)
        env = environment(str(root / "environment"))
        env["QT_QPA_PLATFORM"] = "offscreen"
        env["LIBGL_ALWAYS_SOFTWARE"] = "1"
        process = subprocess.Popen(
            [str(args.firefly.resolve()), str(root / "main.mjs"), "--force-cpu-painting"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
            start_new_session=os.name != "nt",
        )
        try:
            stdout, stderr = process.communicate(timeout=55)
            assert process.returncode == 0 and all(marker in stdout for marker in markers), (
                process.returncode,
                stdout,
                stderr,
            )
            assert "VERIFICATION FAILED" not in stderr, stderr
            print(stdout.strip())
        finally:
            stop(process)
            process.communicate()


if __name__ == "__main__":
    main()
