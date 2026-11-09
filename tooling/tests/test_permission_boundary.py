#!/usr/bin/env python3
"""Test forged native permission identities against a genuine owned offscreen document."""

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
    parser.add_argument("--probe", required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="elytra-permission-boundary-") as temporary:
        root = Path(temporary)
        (root / "boundary-config").mkdir()
        for name in ["boundary.mjs", "boundary.html"]:
            shutil.copyfile(ROOT / "firefly/tests/session-permissions" / name, root / name)
        env = {
            **environment(temporary),
            "QT_QPA_PLATFORM": "offscreen",
            "QT_QPA_PLATFORMTHEME": "generic",
            "ELYTRA_PERMISSION_BOUNDARY_REPORT": str(root / "native-report.json"),
        }
        process = subprocess.Popen(
            [str(args.probe.resolve()), str(root / "boundary.mjs"), "--force-cpu-painting"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
            start_new_session=os.name != "nt",
        )
        try:
            stdout, stderr = process.communicate(timeout=30)
            assert (
                process.returncode == 0
                and "PASS exact native permission boundary and controlled grant revocation twelve cases"
                in stdout
                and "VERIFICATION FAILED" not in stderr
            ), (process.returncode, stdout, stderr)
            print(stdout.strip())
        finally:
            stop(process)
            process.communicate()


if __name__ == "__main__":
    main()
