#!/usr/bin/env python3
"""Run one owned offscreen DOM-port and real utility relay acceptance batch."""

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
    with tempfile.TemporaryDirectory(prefix="firefly-renderer-ports-") as directory:
        root = Path(directory).resolve()
        for name in ("main.mjs", "preload.mjs", "child.mjs", "page.html"):
            shutil.copyfile(ROOT / "firefly/tests/renderer-ports" / name, root / name)
        config = root / "config"
        config.mkdir()
        subprocess.run(
            [
                "node",
                str(ROOT / "tooling/bundle_preload.mjs"),
                str(root / "preload.mjs"),
                str(root / "preload.cjs"),
            ],
            check=True,
        )
        fixture_env = environment(str(config))
        fixture_env["QT_QPA_PLATFORM"] = "offscreen"
        fixture_env["LIBGL_ALWAYS_SOFTWARE"] = "1"
        command = [str(args.firefly.resolve()), str(root / "main.mjs"), "--force-cpu-painting"]
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=fixture_env,
            start_new_session=os.name != "nt",
        )
        try:
            stdout, stderr = process.communicate(timeout=40)
            assert process.returncode == 0 and stdout.count("PASS renderer ports:") == 2, (
                command,
                process.returncode,
                stdout,
                stderr,
            )
            assert "VERIFICATION FAILED" not in stderr, stderr
            print("Firefly offscreen: " + stdout.strip())
        finally:
            stop(process)
            process.communicate()


if __name__ == "__main__":
    main()
