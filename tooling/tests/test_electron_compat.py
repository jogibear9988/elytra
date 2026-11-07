#!/usr/bin/env python3
"""Batch the real native alias/default preload gates; no visible BrowserWindow."""

import _support

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from test_renderer import ROOT, environment, stop
from native_package.runtime_options import verify_electron_compat


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    verify_electron_compat(args.firefly.resolve())
    with tempfile.TemporaryDirectory(prefix="firefly-alias-") as directory:
        root = Path(directory)
        app = root / "app"
        shutil.copytree(ROOT / "firefly/tests/electron-compat", app)
        package = app / "node_modules/electron"
        package.mkdir(parents=True)
        (package / "package.json").write_text('{"main":"index.cjs"}')
        (package / "index.cjs").write_text('module.exports={marker:"NPM_PACKAGE"};\n')
        for compatible in [False, True]:
            command = [str(args.firefly.resolve())]
            if compatible:
                command.append("--electron-compat")
            command += [
                str(app / "main.mjs"),
                "compat" if compatible else "default",
                "--electron-compat",
            ]
            process = subprocess.Popen(
                command,
                env=environment(str(root / str(compatible))),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                start_new_session=True,
            )
            try:
                stdout, stderr = process.communicate(timeout=35)
                assert process.returncode == 0, (
                    command,
                    process.returncode,
                    stdout,
                    stderr[-3000:],
                )
                rows = [json.loads(line) for line in stdout.splitlines() if line.startswith("{")]
                assert rows == [
                    {
                        "compatible": compatible,
                        "sameHost": True,
                        "isolatedPreload": True,
                        "noPagePrivileges": True,
                    }
                ], (stdout, stderr)
            finally:
                stop(process)
                process.communicate()
        print(
            "PASS actual native opt-in/default host alias, npm precedence, stripped flags and isolated preload/page denial"
        )


if __name__ == "__main__":
    main()
