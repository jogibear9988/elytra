#!/usr/bin/env python3
"""Actual native public zoom/layout batch, optionally with a hidden Electron reference."""

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
    parser.add_argument(
        "--firefly", type=Path, help="Exact application objects with offscreen plugin"
    )
    parser.add_argument("--electron", type=Path)
    args = parser.parse_args()
    if not args.firefly and not args.electron:
        parser.error("Require --firefly and/or --electron")
    with tempfile.TemporaryDirectory(prefix="elytra-public-zoom-") as temporary:
        for label, binary in [
            ("Firefly offscreen", args.firefly),
            ("Electron hidden", args.electron),
        ]:
            if binary is None:
                continue
            directory = Path(temporary) / label
            shutil.copytree(ROOT / "firefly/tests/zoom", directory)
            (directory / "config").mkdir()
            reference = label.startswith("Electron")
            if reference:
                for source in directory.glob("*.mjs"):
                    source.write_text(
                        source.read_text().replace("from 'firefly'", "from 'electron'")
                    )
            subprocess.run(
                [
                    "node",
                    str(ROOT / "tooling/bundle_preload.mjs"),
                    str(directory / "preload.mjs"),
                    str(directory / "preload.cjs"),
                ],
                check=True,
            )
            env = environment(str(directory / "config"))
            command = [str(binary.resolve()), str(directory / "main.mjs")]
            if reference:
                command.append("--reference")
            else:
                env["QT_QPA_PLATFORM"] = "offscreen"
                env["LIBGL_ALWAYS_SOFTWARE"] = "1"
                command.append("--force-cpu-painting")
            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                env=env,
                start_new_session=os.name != "nt",
            )
            try:
                stdout, stderr = process.communicate(timeout=35)
                markers = [
                    "PASS public zoom constructor and main setters match real renderer layout DPR and preload getters",
                    "PASS public zoom navigation and reload follow verified native view policy while hidden",
                ]
                if not reference:
                    markers += [
                        "PASS Firefly zoom independent WebContentsView constructor and native renderer state",
                        "PASS Firefly zoom finite range and strict types reject without mutation",
                    ]
                assert process.returncode == 0 and all(marker in stdout for marker in markers), (
                    process.returncode,
                    stdout,
                    stderr,
                )
                assert "VERIFICATION FAILED" not in stderr, stderr
                print(label + ": " + stdout.strip(), flush=True)
            finally:
                stop(process)
                process.communicate()


if __name__ == "__main__":
    main()
