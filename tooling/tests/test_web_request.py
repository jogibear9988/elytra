#!/usr/bin/env python3
"""One hidden genuine webRequest batch per requested runtime; no desktop fallback."""

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
    markers = [
        "PASS public webRequest renderer cancel",
        "PASS public webRequest response header ACAO",
        "PASS public webRequest canonical same cross-origin",
        "PASS public webRequest actual main net",
        "PASS public webRequest custom protocol cancellation",
    ]
    with tempfile.TemporaryDirectory(prefix="elytra-web-request-") as temporary:
        for label, binary in [
            ("Firefly offscreen", args.firefly),
            ("Electron hidden reference", args.electron),
        ]:
            if binary is None:
                continue
            root = Path(temporary) / label
            root.mkdir()
            (root / "config").mkdir()
            for name in ["public.mjs", "page.html", "page.mjs", "payload.txt", "utility-child.mjs"]:
                shutil.copyfile(ROOT / "firefly/tests/web-request" / name, root / name)
            if label.startswith("Electron"):
                entry = root / "public.mjs"
                entry.write_text(entry.read_text().replace("from 'firefly'", "from 'electron'"))
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
                stdout, stderr = process.communicate(timeout=35)
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
