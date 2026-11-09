#!/usr/bin/env python3
"""One hidden real frame tree per requested runtime, with no desktop fallback."""

import _support
import argparse
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
from test_renderer import ROOT, environment, stop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path)
    parser.add_argument("--electron", type=Path)
    args = parser.parse_args()
    if not args.firefly and not args.electron:
        parser.error("Provide --firefly and/or --electron")
    with tempfile.TemporaryDirectory(prefix="elytra-canonical-frames-") as temporary:
        for label, binary in [
            ("Firefly offscreen", args.firefly),
            ("Electron hidden reference", args.electron),
        ]:
            if binary is None:
                continue
            root = Path(temporary) / label
            root.mkdir()
            (root / "config").mkdir()
            for name in ["public.mjs", "page.mjs", "page.html", "preload.mjs"]:
                shutil.copyfile(ROOT / "firefly/tests/frames" / name, root / name)
            if label.startswith("Electron"):
                for name in ["public.mjs", "preload.mjs"]:
                    file = root / name
                    file.write_text(file.read_text().replace("from 'firefly'", "from 'electron'"))
            subprocess.run(
                [
                    "node",
                    str(ROOT / "tooling/bundle_preload.mjs"),
                    str(root / "preload.mjs"),
                    str(root / "preload.cjs"),
                ],
                check=True,
                cwd=ROOT,
            )
            env = environment(str(root / "config"))
            command = [str(binary.resolve()), str(root / "public.mjs")]
            if label.startswith("Firefly"):
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
                stdout, stderr = process.communicate(timeout=30)
                assert process.returncode == 0 and all(
                    marker in stdout
                    for marker in [
                        "PASS public canonical frames before IPC",
                        "PASS public canonical frames same-document",
                        "PASS public canonical IPC senderFrame",
                    ]
                ), (process.returncode, stdout, stderr)
                assert "VERIFICATION FAILED" not in stderr, stderr
                print(label + ": " + stdout.strip(), flush=True)
            finally:
                stop(process)
                process.communicate()


if __name__ == "__main__":
    main()
