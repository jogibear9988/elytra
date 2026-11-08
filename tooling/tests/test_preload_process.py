#!/usr/bin/env python3
"""Check restricted native process metadata, optionally against the Electron reference."""

import _support
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import os
import re
import time
from test_renderer import ROOT, environment, stop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", required=True, type=Path)
    parser.add_argument("--electron", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly-preload-process-") as directory:
        root = Path(directory)
        shutil.copytree(ROOT / "firefly/tests/preload/process", root, dirs_exist_ok=True)
        if args.electron:
            for source in root.glob("*.mjs"):
                source.write_text(source.read_text().replace("from 'firefly'", "from 'electron'"))
        subprocess.run(
            [
                "node",
                str(ROOT / "tooling/bundle_preload.mjs"),
                str(root / "preload.mjs"),
                str(root / "preload.cjs"),
            ],
            check=True,
        )
        binary = (args.electron or args.firefly).resolve()
        command = [str(binary), str(root / "main.mjs"), *(["--reference"] if args.electron else [])]
        fixture_environment = environment(str(root / "config"))
        fixture_environment.pop("FIREFLY_USER_ENV", None)
        with (root / "stdout.log").open("w+") as output, (root / "stderr.log").open("w+") as errors:
            process = subprocess.Popen(
                command,
                stdout=output,
                stderr=errors,
                env=fixture_environment,
                start_new_session=True,
            )
            observed = False
            try:
                deadline = time.monotonic() + 45
                while time.monotonic() < deadline:
                    output.flush()
                    text = (root / "stdout.log").read_text()
                    if not observed and "PROCESS_OBSERVE_READY" in text:
                        pids = list(map(int, re.findall(r"PROCESS_HELPER=(\d+)", text)))[-2:]
                        assert len(pids) == 2 and all(pid > 0 for pid in pids), text
                        if os.name != "nt":
                            for pid in set(pids):
                                parent = subprocess.check_output(
                                    ["ps", "-o", "ppid=", "-p", str(pid)], text=True
                                )
                                assert int(parent.strip()) == process.pid, (
                                    pid,
                                    parent,
                                    process.pid,
                                )
                        observed = True
                    if process.poll() is not None:
                        break
                    time.sleep(0.025)
                else:
                    raise AssertionError(
                        (
                            "Sandbox process fixture timed out",
                            text,
                            (root / "stderr.log").read_text(),
                        )
                    )
                errors.flush()
                stderr = (root / "stderr.log").read_text()
                assert process.returncode == 0 and observed and "PASS sandbox process:" in text, (
                    text,
                    stderr,
                )
                assert "VERIFICATION FAILED" not in stderr, stderr
                print(("Electron reference: " if args.electron else "") + text.strip())
            finally:
                stop(process)
                process.wait()


if __name__ == "__main__":
    main()
